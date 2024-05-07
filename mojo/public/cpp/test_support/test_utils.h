// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_TEST_SUPPORT_TEST_UTILS_H_
#define MOJO_PUBLIC_CPP_TEST_SUPPORT_TEST_UTILS_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/types/to_address.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

namespace mojo {

class MessagePipeHandle;

namespace test {

// This overload is used for mojom structures with struct traits. The C++
// structure type is given as an input, and returned as an output.
template <typename MojomType,
          typename UserStructType,
          std::enable_if_t<
              !std::is_same<mojo::InlinedStructPtr<MojomType>,
                            std::remove_const_t<UserStructType>>::value &&
                  !std::is_same<mojo::StructPtr<MojomType>,
                                std::remove_const_t<UserStructType>>::value &&
                  !std::is_enum<UserStructType>::value,
              int> = 0>
bool SerializeAndDeserialize(UserStructType& input,
                             std::remove_const_t<UserStructType>& output) {
  mojo::Message message = MojomType::SerializeAsMessage(&input);

  // This accurately simulates full serialization to ensure that all attached
  // handles are serialized as well. Necessary for DeserializeFromMessage to
  // work properly.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  message = mojo::Message::CreateFromMessageHandle(&handle);
  DCHECK(!message.IsNull());

  return MojomType::DeserializeFromMessage(std::move(message), &output);
}

// This overload is used for mojom structures with struct traits, but here they
// are serialized from a manually constructed StructPtr instead of from the C++
// structure using the struct traits. This allows malformed data to be put in
// the StructPtr<MojomType>, in order to verify the behaviour of deserialization
// back to the C++ structure type.
template <typename MojomType,
          typename UserStructType,
          typename MojomStructPtr,
          std::enable_if_t<
              (std::is_same<mojo::InlinedStructPtr<MojomType>,
                            std::remove_const_t<MojomStructPtr>>::value ||
               std::is_same<mojo::StructPtr<MojomType>,
                            std::remove_const_t<MojomStructPtr>>::value) &&
                  !std::is_enum<UserStructType>::value,
              int> = 0>
bool SerializeAndDeserialize(MojomStructPtr& input, UserStructType& output) {
  mojo::Message message = MojomType::SerializeAsMessage(&input);

  // This accurately simulates full serialization to ensure that all attached
  // handles are serialized as well. Necessary for DeserializeFromMessage to
  // work properly.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  message = mojo::Message::CreateFromMessageHandle(&handle);
  DCHECK(!message.IsNull());

  return MojomType::DeserializeFromMessage(std::move(message), &output);
}

// This overload is used for mojom enums. The C++ enum type is given as an
// input, and returned as an output.
template <typename MojomType,
          typename UserEnumType,
          std::enable_if_t<std::is_enum<UserEnumType>::value, int> = 0>
bool SerializeAndDeserialize(UserEnumType input, UserEnumType& output) {
  MojomType mode = mojo::EnumTraits<MojomType, UserEnumType>::ToMojom(input);
  return mojo::EnumTraits<MojomType, UserEnumType>::FromMojom(mode, &output);
}

// Writes a message to |handle| with message data |text|. Returns true on
// success.
bool WriteTextMessage(const MessagePipeHandle& handle, const std::string& text);

// Reads a message from |handle|, putting its contents into |*text|. Returns
// true on success. (This blocks if necessary and will call |MojoReadMessage()|
// multiple times, e.g., to query the size of the message.)
bool ReadTextMessage(const MessagePipeHandle& handle, std::string* text);

// Discards a message from |handle|. Returns true on success. (This does not
// block. It will fail if no message is available to discard.)
bool DiscardMessage(const MessagePipeHandle& handle);

// Run |single_iteration| an appropriate number of times and report its
// performance appropriately. (This actually runs |single_iteration| for a fixed
// amount of time and reports the number of iterations per unit time.)
typedef void (*PerfTestSingleIteration)(void* closure);
void IterateAndReportPerf(const char* test_name,
                          const char* sub_test_name,
                          PerfTestSingleIteration single_iteration,
                          void* closure);

// Intercepts a single bad message (reported via mojo::ReportBadMessage or
// mojo::GetBadMessageCallback) that would be associated with the global bad
// message handler (typically when the messages originate from a test
// implementation of an interface hosted in the test process).
class BadMessageObserver {
 public:
  BadMessageObserver();

  BadMessageObserver(const BadMessageObserver&) = delete;
  BadMessageObserver& operator=(const BadMessageObserver&) = delete;

  ~BadMessageObserver();

  // Waits for the bad message and returns the error string.
  std::string WaitForBadMessage();

  // Returns true iff a bad message was already received.
  bool got_bad_message() const { return got_bad_message_; }

 private:
  void OnReportBadMessage(const std::string& message);

  std::string last_error_for_bad_message_;
  bool got_bad_message_;
  base::RunLoop run_loop_;
};

// Test helper for swapping out a Mojo implementation pointer for testing in a
// given scope. It is possible to nest interceptions on a given receiver;
// however, the scopers must be destroyed in reverse order of creation.
//
// Usage example:
//
// // Basic portal implementation.
// class PortalImpl : public mojom::Portal {
//  public:
//   auto& receiver_for_testing() { return receiver_; }
//
//   private:
//    mojo::Receiver<mojom::Portal> receiver_;
// };
//
// // In unit test:
// class PortalTester : public mojom::PortalInterceptorForTesting {
//  public:
//   explicit PortalTester(PortalImpl* impl)
//       : swapped_impl_(impl->receiver_for_testing(), impl) {}
//
//   // PortalInterceptorForTesting overrides:
//   mojom::Portal* GetForwardingInterface() override {
//     // Important: do not just return the `impl` passed in the constructor; if
//     // the receiver is already intercepted, this will result in the
//     // previously-installed interceptor being skipped!
//     return swapped_impl_.old_impl();
//   }
//
//  private:
//   mojo::test::ScopedSwapImplForTesting<mojom::Portal> swapped_impl_;
// };
// TEST_F(PortalTest, Basic) {
//   PortalTester tester{GetPortal()};
//
//   // <test code here>
// }
//
// Postconditions: the caller must destroy `ScopedSwapImplForTesting` before the
// targeted receiver goes out of scope.
//
// TODO(dcheng): Consider adding a way to cancel the reset at destruction for
// test cases that need this.
template <typename T>
class ScopedSwapImplForTesting {
 public:
  template <typename Receiver>
  ScopedSwapImplForTesting(Receiver& receiver,
                           typename Receiver::ImplPointerType new_impl) {
    using ImplPtr = typename Receiver::ImplPointerType;
    T* new_impl_ptr = base::to_address(new_impl);
    ImplPtr old_impl = receiver.SwapImplForTesting(std::move(new_impl));
    old_impl_ptr_ = base::to_address(old_impl);
    closure_ = base::BindOnce(
        [](Receiver& receiver, ImplPtr old_impl, T* new_impl_ptr) {
          CHECK_EQ(new_impl_ptr, base::to_address(receiver.SwapImplForTesting(
                                     std::move(old_impl))))
              << "Mismatched impl pointers when restoring implementation; this "
              << "indicates a bug where a receiver was multiply-intercepted "
              << "in tests, but the ScopedSwapImplForTesting instances were "
              << "not destroyed in reverse order of creation.";
        },
        std::ref(receiver), std::move(old_impl), new_impl_ptr);
  }

  ~ScopedSwapImplForTesting() { std::move(closure_).Run(); }

  T* old_impl() const { return old_impl_ptr_; }

  ScopedSwapImplForTesting(const ScopedSwapImplForTesting&) = delete;
  ScopedSwapImplForTesting& operator=(const ScopedSwapImplForTesting&) = delete;

 private:
  raw_ptr<T> old_impl_ptr_;
  base::OnceClosure closure_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_TEST_SUPPORT_TEST_UTILS_H_

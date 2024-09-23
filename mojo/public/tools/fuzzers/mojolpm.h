// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TOOLS_FUZZERS_MOJOLPM_H_
#define MOJO_PUBLIC_TOOLS_FUZZERS_MOJOLPM_H_

#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/tools/fuzzers/mojolpm.pb.h"

#define MOJOLPM_DBG 0
#if MOJOLPM_DBG
#define mojolpmdbg(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)
#else
#define mojolpmdbg(msg, ...)
#endif

namespace mojolpm {

typedef void* TypeId;

// Returns a unique TypeId for every different typename T.
template <typename T>
TypeId type_id() {
  static std::remove_reference<T>* ptr = nullptr;
  return &ptr;
}

#if MOJOLPM_DBG
template <typename T>
std::string type_name() {
  return std::string(__PRETTY_FUNCTION__)
      .substr(38, strlen(__PRETTY_FUNCTION__) - 39);
}
#endif

// Common state shared between generated MojoLPM code and fuzzer-specific
// testcases.
//
// The main state is (various) object instances used as parameters and response
// parameters to Mojo method calls, along with the instances of Mojo Remote and
// Receiver objects.
//
// At code-generation time we don't have access to all of the types used, we
// can't know ahead of time all of the types that might be stored. This is
// further complicated by the move-only and sequence-bound semantics of some
// Mojo objects.
//
// The interface
//   AddInstance<T>(), GetInstance<T>(), RemoveInstance<T>(), etc.
// implements a per-type mapping from integer id to instance of type T. Adding a
// new instance can be performed with or without providing an id; in the case
// that a fuzzer attempts to store two instances of the same type with the same
// id at once, the second instance will be immediately destroyed.
//
// We want most calls to GetInstance to return a valid object, even if there
// wasn't an object previously stored with the given id (lookup ids are
// randomly generated during fuzzing), so GetInstance does a very fuzzy lookup.
//
// The details of the algorithms used to generate a new id, and match the id
// when there is no exact match should not be depended on. If a fuzzer needs to
// know the id that will be used to store an object, but does not have a
// testcase-supplied id, then the function NextId<T>() should be used to choose
// an id.
//
// All methods should only be called from the fuzzer sequence.
class Context {
 public:
  Context();
  Context(const Context&) = delete;
  ~Context();

  // Lookup the previously stored instance of type T using a fuzzy-match on the
  // provided id. Returns nullptr if there's no matching instance.
  template <typename T>
  T* GetInstance(uint32_t id);

  // Lookup the previously stored instance of type T using a fuzzy-match on
  // the provided id, and remove that instance from the object storage, passing
  // ownership of that instance to the caller. Returns nullptr if there's no
  // matching instance.
  template <typename T>
  std::unique_ptr<T> GetAndRemoveInstance(uint32_t id);

  // Lookup the previously stored instance of type T using a fuzzy-match on
  // the provided id, and remove that instance from the object storage.
  template <typename T>
  void RemoveInstance(uint32_t id);

  // Adds an instance of type T to the object storage using an automatically
  // chosen id, which it returns. Equivalent to calling
  //   AddInstance<T>(NextId<T>(), instance);
  template <typename T>
  uint32_t AddInstance(T instance);

  // Adds an instance of type T to the object storage using the provided id. If
  // the provided id already exists in the object storage, the existing instance
  // is not modified, and the implementation will assign a new id. Returns the
  // id that can be used to lookup the instance.
  template <typename T>
  uint32_t AddInstance(uint32_t id, T instance);

  // Returns an instance id for the given type T that is guaranteed to be
  // available for storing an instance of type T at the time of calling.
  //
  // NB: This does NOT reserve the id; so the following snippet is not correct.
  //
  // uint32_t id = NextId<T>();
  // AddInstance<T>(some_t);
  // CHECK_EQ(id, AddInstance<T>(id, some_t));
  template <typename T>
  uint32_t NextId();

  // Starts a deserialization section. This is needed because associated binding
  // types will only become valid once they are sent over a message pipe, and
  // this means that we need to be able to rollback any instances added if later
  // deserialization fails.
  void StartDeserialization();

  // Enum used to make the expected behaviour of EndDeserialization calls clear
  // from the callsites.
  enum class Rollback {
    kNoRollback,
    kRollback,
  };

  // Ends a deserialization section. If `kRollback`, then any associated binding
  // instances added since the last call to StartDeserialization will be removed
  // from the instance storage.
  void EndDeserialization(Rollback rollback);

  void StartTestcase();
  void EndTestcase();

 private:
  // Lookup the previously stored instance of type T using a fuzzy-match on
  // the provided id, and remove that instance from the object storage.
  void RemoveInstance(TypeId type_id, uint32_t id);

  // mojolpm::Context::Storage implements generic storage for all possible
  // object types that might be created during fuzzing. This allows the fuzzer
  // to reference objects by id, even when the possible types of those objects
  // are only known at fuzzer compile time.

  struct Storage {
    Storage();

    template <typename T>
    explicit Storage(T&& value);

    ~Storage();

    struct StorageWrapperBase {
      virtual ~StorageWrapperBase() = default;
      virtual TypeId type() const = 0;
    };

    template <typename T>
    struct StorageWrapper : StorageWrapperBase {
      StorageWrapper() = default;
      explicit StorageWrapper(T&& value);

      TypeId type() const override;
      T& value();
      const T& value() const;

     private:
      T value_;
    };

    template <typename T>
    T& get();

    template <typename T>
    const T& get() const;

    template <typename T>
    std::unique_ptr<T> release();

   private:
    std::unique_ptr<StorageWrapperBase> wrapper_;
  };

  // mojolpm::Context::StorageTraits implements type-specific details in the
  // handling of stored instances. In particular, we need to guarantee that
  // certain types are destroyed before other types - all fuzzer-owned
  // mojo::Remote and mojo::Receiver objects need to be destroyed before any
  // callbacks can be safely destroyed.

  template <typename T>
  class StorageTraits {
   public:
    explicit StorageTraits(Context* context) {}
    void OnInstanceAdded(uint32_t id) {}
  };

  template <typename T>
  class StorageTraits<::mojo::Remote<T>> {
   public:
    explicit StorageTraits(Context* context) : context_(context) {}
    void OnInstanceAdded(uint32_t id);

   private:
    Context* context_;
  };

  template <typename T>
  class StorageTraits<::mojo::AssociatedRemote<T>> {
   public:
    explicit StorageTraits(Context* context) : context_(context) {}
    void OnInstanceAdded(uint32_t id);

   private:
    Context* context_;
  };

  template <typename T>
  class StorageTraits<std::unique_ptr<::mojo::Receiver<T>>> {
   public:
    explicit StorageTraits(Context* context) : context_(context) {}
    void OnInstanceAdded(uint32_t id);

   private:
    Context* context_;
  };

  template <typename T>
  class StorageTraits<std::unique_ptr<::mojo::AssociatedReceiver<T>>> {
   public:
    explicit StorageTraits(Context* context) : context_(context) {}
    void OnInstanceAdded(uint32_t id);

   private:
    Context* context_;
  };

  std::map<TypeId, std::map<uint32_t, Storage>> instances_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::set<TypeId> interface_type_ids_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<std::pair<TypeId, uint32_t>> rollback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

Context* GetContext();

template <typename T>
Context::Storage::Storage(T&& value) {
  wrapper_ = std::make_unique<StorageWrapper<T>>(std::forward<T>(value));
}

template <typename T>
T& Context::Storage::Storage::get() {
  DCHECK(wrapper_->type() == type_id<T>());
  DCHECK(static_cast<StorageWrapper<T>*>(wrapper_.get()));
  return static_cast<StorageWrapper<T>*>(wrapper_.get())->value();
}

template <typename T>
const T& Context::Storage::Storage::get() const {
  DCHECK(wrapper_->type() == type_id<T>());
  DCHECK(static_cast<StorageWrapper<T>*>(wrapper_.get()));
  return static_cast<StorageWrapper<T>*>(wrapper_.get())->value();
}

template <typename T>
std::unique_ptr<T> Context::Storage::Storage::release() {
  DCHECK(wrapper_->type() == type_id<T>());
  DCHECK(static_cast<StorageWrapper<T>*>(wrapper_.get()));
  return std::make_unique<T>(
      std::move(static_cast<StorageWrapper<T>*>(wrapper_.get())->value()));
}

template <typename T>
Context::Storage::StorageWrapper<T>::StorageWrapper(T&& value)
    : value_(std::move(value)) {}

template <typename T>
TypeId Context::Storage::StorageWrapper<T>::type() const {
  return type_id<T>();
}

template <typename T>
T& Context::Storage::StorageWrapper<T>::value() {
  return value_;
}

template <typename T>
const T& Context::Storage::StorageWrapper<T>::value() const {
  return value_;
}

template <typename T>
void Context::StorageTraits<::mojo::Remote<T>>::OnInstanceAdded(uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(context_->sequence_checker_);
  context_->interface_type_ids_.insert(type_id<::mojo::Remote<T>>());
  auto instance = context_->GetInstance<::mojo::Remote<T>>(id);
  context_->rollback_.emplace_back(type_id<::mojo::Remote<T>>(), id);
  CHECK(instance);
  // Unretained is safe here since context_ owns instance.
  instance->set_disconnect_handler(
      base::BindOnce(&Context::RemoveInstance<::mojo::Remote<T>>,
                     base::Unretained(context_), id));
}

template <typename T>
void Context::StorageTraits<::mojo::AssociatedRemote<T>>::OnInstanceAdded(
    uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(context_->sequence_checker_);
  context_->interface_type_ids_.insert(type_id<::mojo::AssociatedRemote<T>>());
  auto instance = context_->GetInstance<::mojo::AssociatedRemote<T>>(id);
  context_->rollback_.emplace_back(type_id<::mojo::AssociatedRemote<T>>(), id);
  CHECK(instance);
  // Unretained is safe here since context_ owns instance.
  instance->set_disconnect_handler(
      base::BindOnce(&Context::RemoveInstance<::mojo::AssociatedRemote<T>>,
                     base::Unretained(context_), id));
}

template <typename T>
void Context::StorageTraits<
    std::unique_ptr<::mojo::Receiver<T>>>::OnInstanceAdded(uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(context_->sequence_checker_);
  context_->interface_type_ids_.insert(
      type_id<std::unique_ptr<::mojo::Receiver<T>>>());
  auto instance =
      context_->GetInstance<std::unique_ptr<::mojo::Receiver<T>>>(id);
  CHECK(instance);
  // Unretained is safe here since context_ owns instance.
  (*instance)->set_disconnect_handler(base::BindOnce(
      &Context::RemoveInstance<std::unique_ptr<::mojo::Receiver<T>>>,
      base::Unretained(context_), id));
}

template <typename T>
void Context::StorageTraits<std::unique_ptr<::mojo::AssociatedReceiver<T>>>::
    OnInstanceAdded(uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(context_->sequence_checker_);
  context_->interface_type_ids_.insert(
      type_id<std::unique_ptr<::mojo::AssociatedReceiver<T>>>());
  auto instance =
      context_->GetInstance<std::unique_ptr<::mojo::AssociatedReceiver<T>>>(id);
  CHECK(instance);
  // Unretained is safe here since context_ owns instance.
  (*instance)->set_disconnect_handler(base::BindOnce(
      &Context::RemoveInstance<std::unique_ptr<::mojo::AssociatedReceiver<T>>>,
      base::Unretained(context_), id));
}

template <typename T>
T* Context::GetInstance(uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojolpmdbg("getInstance(%s, %i) = ", type_name<T>().c_str(), id);
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter == instances_.end()) {
    mojolpmdbg("failed!\n");
    return nullptr;
  } else {
    auto& instance_map = instances_iter->second;

    // normalize id to [0, max_id]
    if (instance_map.size() > 0 && instance_map.rbegin()->first < id) {
      id = id % (instance_map.rbegin()->first + 1);
    }

    // choose the first valid entry after id
    auto instance = instance_map.lower_bound(id);
    if (instance == instance_map.end()) {
      mojolpmdbg("failed!\n");
      return nullptr;
    }

    mojolpmdbg("%i\n", instance->first);
    return &instance->second.template get<T>();
  }
}

template <typename T>
std::unique_ptr<T> Context::GetAndRemoveInstance(uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojolpmdbg("getAndRemoveInstance(%s, %i) = ", type_name<T>().c_str(), id);
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter == instances_.end()) {
    mojolpmdbg("failed\n");
    return nullptr;
  } else {
    auto& instance_map = instances_iter->second;

    // normalize id to [0, max_id]
    if (instance_map.size() > 0 && instance_map.rbegin()->first < id) {
      id = id % (instance_map.rbegin()->first + 1);
    }

    // choose the first valid entry after id
    auto instance = instance_map.lower_bound(id);
    if (instance == instance_map.end()) {
      mojolpmdbg("failed!\n");
      return nullptr;
    }

    mojolpmdbg("%i\n", instance->first);
    auto result = instance->second.template release<T>();
    instance_map.erase(instance);
    return std::move(result);
  }
}

template <typename T>
void Context::RemoveInstance(uint32_t id) {
  mojolpmdbg("RemoveInstance(%s, %u) = ", type_name<T>().c_str(), id);
  RemoveInstance(type_id<T>(), id);
}

template <typename T>
uint32_t Context::AddInstance(T instance) {
  return AddInstance(1, std::move(instance));
}

template <typename T>
uint32_t Context::AddInstance(uint32_t id, T instance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter == instances_.end()) {
    instances_[type_id<T>()].emplace(id, std::move(instance));
  } else {
    auto& instance_map = instances_iter->second;
    auto instance_map_iter = instance_map.find(id);
    // if this id is a collision with an existing entry, loop until we find a
    // free id.
    while (instance_map_iter != instance_map.end()) {
      id = instance_map_iter->first + 1;
      instance_map_iter = instance_map.find(id);
    }
    instance_map.emplace(id, std::move(instance));
  }
  mojolpmdbg("addInstance(%s, %u)\n", type_name<T>().c_str(), id);
  StorageTraits<T> traits(this);
  traits.OnInstanceAdded(id);
  return id;
}

template <typename T>
uint32_t Context::NextId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t id = 1;
  auto instances_iter = instances_.find(type_id<T>());
  if (instances_iter != instances_.end()) {
    auto& instance_map = instances_iter->second;
    auto instance_map_iter = instance_map.find(id);
    // if this id is a collision with an existing entry, loop until we find a
    // free id.
    while (instance_map_iter != instance_map.end()) {
      id = instance_map_iter->first + 1;
      instance_map_iter = instance_map.find(id);
    }
  }
  return id;
}

template <typename T>
std::unique_ptr<mojo::Remote<T>> NewRemote() {
  // mojolpmdbg("default NewInstance<>\n");
  return nullptr;
}

template <typename T>
std::unique_ptr<mojo::AssociatedRemote<T>> NewAssociatedRemote() {
  // mojolpmdbg("default NewInstanceA<>\n");
  return nullptr;
}

template <typename T>
uint32_t NextId() {
  return GetContext()->NextId<T>();
}

// Common code shared between most fuzzer testcases.
//
// Per-testcase state needed to run the interface being tested. The lifetime of
// this is scoped to a single testcase, and it is created and destroyed from the
// fuzzer sequence.
//
// This removes the need for a lot of boilerplate code to handle the most basic
// actions that almost all MojoLPM fuzzers will want to perform. It assumes some
// common formatting for testcase protobufs, which can be seen in (for example)
// content/test/fuzzers/code_cache_host_mojolpm_fuzzer.proto:
//
// message Action {
//   oneof action {
//     ... fuzzer actions here
//   }
// }
//
// message Sequence {
//   repeated uint32 action_indexes = 1 [packed = true];
// }
//
// message Testcase {
//   repeated Action actions = 1;
//   repeated Sequence sequences = 2;
//   repeated uint32 sequence_indexes = 3 [packed = true];
// }
//
// In this case, the individual fuzzer testcase type should then inherit from
//   mojolpm::Testcase<proto::Testcase, proto::Action>
// and override `SetUp`, `TearDown` and `DoAction` accordingly.
//
// See content/test/fuzzer/code_cache_host_mojolpm_fuzzer.cc for an example.
//
// The fuzzer main function should then set up the environment and post a call
// to mojolpm::RunTestcase onto the fuzzer thread.
template <typename ProtoTestcase,
          typename ProtoAction,
          int kMaxActionCount = 128,
          int kMaxActionSize = 300 * 1024 * 1024>
class Testcase {
 protected:
  explicit Testcase(const ProtoTestcase& proto_testcase);

  // The three functions below need to be implemented for each fuzzer.

  // SetUp will be invoked prior to the first fuzzer actions running; and once
  // it has completed, all per-testcase fuzzer state should be ready.
  //
  // Once setup is complete, `done_closure` should be invoked on the fuzzer
  // sequence.
  //
  // This will be called from the fuzzer sequence.
  virtual void SetUp(base::OnceClosure done_closure) = 0;

  // TearDown will be invoked after the last fuzzer action has run; and once it
  // has completed, all per-testcase fuzzer state should have been destroyed
  // (and the process should be left in a state that would permit a new testcase
  // to start).
  //
  // Once teardown is complete, `done_closure` should be invoked on the fuzzer
  // sequence.
  //
  // This will be called from the fuzzer sequence.
  virtual void TearDown(base::OnceClosure done_closure) = 0;

  // RunAction will be invoked for each action as described by the protobuf file
  // for the testcase. This function is responsible for executing the given
  // action, and then invoking `done_closure`. `done_closure` must be invoked
  // regardless of any failure/error condition that may occur.
  //
  // For the most part this will be a large switch statement that simply hands
  // actions to the appropriate mojolpm function; but it will also need to
  // handle the initial interface binding for the interfaces being fuzzed.
  //
  // case ProtoAction::kFooRemoteAction:
  //   mojolpm::HandleRemoteAction(action.foo_remote_action());
  //   break;
  //
  // case ProtoAction::kBarReceiverAction:
  //   mojolpm::HandleReceiverAction(action.bar_receiver_action());
  //   break;
  //
  // Once the action has been executed, `done_closure` should be invoked on the
  // fuzzer sequence.
  //
  // This will be called from the fuzzer sequence.
  virtual void RunAction(const ProtoAction& action,
                         base::OnceClosure done_closure) = 0;

 private:
  template <typename T>
  friend void RunTestcase(
      T* testcase,
      scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner,
      base::OnceClosure done_closure);

  // While there are still actions remaining in the testcase, this will perform
  // the next action, and then queue itself to run again. When the testcase is
  // finished, this will invoke `done_closure` on the fuzzer sequence.
  //
  // This should only be called from the fuzzer sequence.
  void Run(scoped_refptr<base::TaskRunner> fuzzer_task_runner,
           base::OnceClosure done_closure);

  // Returns true once either all of the actions in the testcase have been
  // performed, or the per-testcase action limit has been exceeded.
  //
  // This should only be called from the fuzzer sequence.
  bool IsFinished();

  // The proto message describing the test actions to perform.
  const ProtoTestcase& proto_testcase_;

  // Count of total actions performed in this testcase.
  int action_count_ = 0;

  // The index of the next sequence of actions to execute.
  int next_sequence_idx_ = 0;

  // The index of the next action to execute.
  int next_action_idx_ = 0;

 protected:
  SEQUENCE_CHECKER(sequence_checker_);
};

template <typename ProtoTestcase,
          typename ProtoAction,
          int kMaxActionCount,
          int kMaxActionSize>
Testcase<ProtoTestcase, ProtoAction, kMaxActionCount, kMaxActionSize>::Testcase(
    const ProtoTestcase& proto_testcase)
    : proto_testcase_(proto_testcase) {
  // Testcase's are created on the main thread, but the actions that we want to
  // validate the sequencing of take place on the fuzzer sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename ProtoTestcase,
          typename ProtoAction,
          int kMaxActionCount,
          int kMaxActionSize>
bool Testcase<ProtoTestcase, ProtoAction, kMaxActionCount, kMaxActionSize>::
    IsFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!proto_testcase_.actions_size() || !proto_testcase_.sequences_size()) {
    return true;
  }

  if (next_sequence_idx_ >= proto_testcase_.sequence_indexes_size()) {
    return true;
  }

  if (action_count_ >= kMaxActionCount) {
    return true;
  }

  return false;
}

template <typename ProtoTestcase,
          typename ProtoAction,
          int kMaxActionCount,
          int kMaxActionSize>
void Testcase<ProtoTestcase, ProtoAction, kMaxActionCount, kMaxActionSize>::Run(
    scoped_refptr<base::TaskRunner> fuzzer_task_runner,
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsFinished()) {
    std::move(done_closure).Run();

    // Explicit return early here, since `this` will be invalidated as soon as
    // `done_closure` is invoked.
    return;
  } else {
    // Bind a closure to continue the fuzzing. This must be called in every path
    // through this block, otherwise fuzzing will hang. Unretained is safe since
    // `this` will only be destroyed after `done_closure` is called.
    auto run_closure = base::BindOnce(
        &Testcase<ProtoTestcase, ProtoAction, kMaxActionCount,
                  kMaxActionSize>::Run,
        base::Unretained(this), fuzzer_task_runner, std::move(done_closure));

    auto sequence_idx = proto_testcase_.sequence_indexes(next_sequence_idx_);
    const auto& sequence = proto_testcase_.sequences(
        sequence_idx % proto_testcase_.sequences_size());
    if (next_action_idx_ < sequence.action_indexes_size()) {
      auto action_idx = sequence.action_indexes(next_action_idx_++);
      const auto& action =
          proto_testcase_.actions(action_idx % proto_testcase_.actions_size());
      if (action.ByteSizeLong() <= kMaxActionSize) {
        RunAction(action, std::move(run_closure));
      } else {
        fuzzer_task_runner->PostTask(FROM_HERE, std::move(run_closure));
      }
    } else {
      next_sequence_idx_++;
      next_action_idx_ = 0;
      fuzzer_task_runner->PostTask(FROM_HERE, std::move(run_closure));
    }
  }
}

// Helper function to setup and run the testcase, since we need to do that from
// the fuzzer sequence rather than the main thread.
template <typename T>
void RunTestcase(T* testcase,
                 scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner,
                 base::OnceClosure done_closure) {
  auto teardown = base::BindOnce(&T::TearDown, base::Unretained(testcase),
                                 std::move(done_closure));

  auto start_fuzzing = base::BindOnce(&T::Run, base::Unretained(testcase),
                                      fuzzer_task_runner, std::move(teardown));

  fuzzer_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&T::SetUp, base::Unretained(testcase),
                                std::move(start_fuzzing)));
}

bool FromProto(const bool& input, bool& output);
bool ToProto(const bool& input, bool& output);
bool FromProto(const ::google::protobuf::int32& input, int8_t& output);
bool ToProto(const int8_t& input, ::google::protobuf::int32& output);
bool FromProto(const ::google::protobuf::int32& input, int16_t& output);
bool ToProto(const int16_t& input, ::google::protobuf::int32& output);
bool FromProto(const ::google::protobuf::int32& input, int32_t& output);
bool ToProto(const int32_t& input, ::google::protobuf::int32& output);
bool FromProto(const ::google::protobuf::int64& input, int64_t& output);
bool ToProto(const int64_t& input, ::google::protobuf::int64& output);
bool FromProto(const ::google::protobuf::uint32& input, uint8_t& output);
bool ToProto(const uint8_t& input, ::google::protobuf::uint32& output);
bool FromProto(const ::google::protobuf::uint32& input, uint16_t& output);
bool ToProto(const uint16_t& input, ::google::protobuf::uint32& output);
bool FromProto(const ::google::protobuf::uint32& input, uint32_t& output);
bool ToProto(const uint32_t& input, ::google::protobuf::uint32& output);
bool FromProto(const ::google::protobuf::uint64& input, uint64_t& output);
bool ToProto(const uint64_t& input, ::google::protobuf::uint64& output);
bool FromProto(const double& input, double& output);
bool ToProto(const double& input, double& output);
bool FromProto(const float& input, float& output);
bool ToProto(const float& input, float& output);
bool FromProto(const std::string& input, std::string& output);
bool ToProto(const std::string& input, std::string& output);
bool FromProto(const ::mojolpm::Handle& input, mojo::ScopedHandle& output);
bool ToProto(const mojo::ScopedHandle& input, ::mojolpm::Handle& output);
bool FromProto(const ::mojolpm::DataPipeConsumerHandle& input,
               mojo::ScopedDataPipeConsumerHandle& output);
bool ToProto(const mojo::ScopedDataPipeConsumerHandle& input,
             ::mojolpm::DataPipeConsumerHandle& output);
bool FromProto(const ::mojolpm::DataPipeProducerHandle& input,
               mojo::ScopedDataPipeProducerHandle& output);
bool ToProto(const mojo::ScopedDataPipeProducerHandle& input,
             ::mojolpm::DataPipeProducerHandle& output);
bool FromProto(const ::mojolpm::MessagePipeHandle& input,
               mojo::ScopedMessagePipeHandle& output);
bool ToProto(const mojo::ScopedMessagePipeHandle& input,
             ::mojolpm::MessagePipeHandle& output);
bool FromProto(const ::mojolpm::SharedBufferHandle& input,
               mojo::ScopedSharedBufferHandle& output);
bool ToProto(const mojo::ScopedSharedBufferHandle& input,
             ::mojolpm::SharedBufferHandle& output);
bool FromProto(const ::mojolpm::PlatformHandle& input,
               mojo::PlatformHandle& output);
bool ToProto(const mojo::PlatformHandle& input,
             ::mojolpm::PlatformHandle& output);

void HandleDataPipeRead(const ::mojolpm::DataPipeRead& input);
void HandleDataPipeWrite(const ::mojolpm::DataPipeWrite& input);
void HandleDataPipeConsumerClose(const ::mojolpm::DataPipeConsumerClose& input);
void HandleDataPipeProducerClose(const ::mojolpm::DataPipeProducerClose& input);
void HandleSharedBufferWrite(const ::mojolpm::SharedBufferWrite& input);
void HandleSharedBufferRelease(const ::mojolpm::SharedBufferRelease& input);
}  // namespace mojolpm

#endif  // MOJO_PUBLIC_TOOLS_FUZZERS_MOJOLPM_H_

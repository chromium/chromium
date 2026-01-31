// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#include <winsock2.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/winsock_init.h"

namespace net {

namespace {

class AddressSorterWin : public AddressSorter {
 public:
  AddressSorterWin() {
    EnsureWinsockInit();
  }

  AddressSorterWin(const AddressSorterWin&) = delete;
  AddressSorterWin& operator=(const AddressSorterWin&) = delete;

  ~AddressSorterWin() override {}

  // AddressSorter:
  void Sort(const std::vector<IPEndPoint>& endpoints,
            CallbackType callback) const override {
    DCHECK(!endpoints.empty());
    Job::Start(endpoints, std::move(callback));
  }

 private:
  // Executes the SIO_ADDRESS_LIST_SORT ioctl asynchronously, and
  // performs the necessary conversions to/from `std::vector<IPEndPoint>`.
  class Job : public base::RefCountedThreadSafe<Job> {
   public:
    static void Start(const std::vector<IPEndPoint>& endpoints,
                      CallbackType callback) {
      auto job = base::WrapRefCounted(new Job(endpoints, std::move(callback)));
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&Job::Run, job),
          base::BindOnce(&Job::OnComplete, job));
    }

    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

   private:
    friend class base::RefCountedThreadSafe<Job>;

    // Helper to calculate the buffer size before the members are initialized.
    static size_t CalculateBufferSize(size_t address_count) {
      auto checked_size =
          base::CheckedNumeric<size_t>(sizeof(SOCKET_ADDRESS_LIST)) +
          base::CheckedNumeric<size_t>(address_count) *
              (sizeof(SOCKET_ADDRESS) + sizeof(SOCKADDR_STORAGE));
      return checked_size.ValueOrDie();
    }

    Job(const std::vector<IPEndPoint>& endpoints, CallbackType callback)
        : callback_(std::move(callback)),
          input_buffer_storage_(base::HeapArray<uint8_t>::WithSize(
              CalculateBufferSize(endpoints.size()))),
          output_buffer_storage_(base::HeapArray<uint8_t>::WithSize(
              input_buffer_storage_.size())) {
      const size_t address_count = endpoints.size();

      // `input_buffer_storage_` has the following structure:
      // SOCKET_ADDRESS_LIST, which has an INT address count followed by a
      // an array of `address_count` SOCKET_ADDRESS's.
      // An array of `address_count` SOCKADDR_STORAGE's.
      // Each SOCKET_ADDRESS has a pointer to its corresponding SOCKADDR_STORAGE
      // in `input_buffer_storage`, as well as the length of the address, which
      // will be <= sizeof(SOCKADR_STORAGE).
      auto* input_list =
          reinterpret_cast<SOCKET_ADDRESS_LIST*>(input_buffer_storage_.data());
      input_list->iAddressCount = base::checked_cast<INT>(address_count);

      // Create a span of SOCKET_ADDRESS_LIST objects in `input_list`.
      // SAFETY: This is safe because `input_list` was derived from
      // `input_buffer_storage_`which included space for `address_count` address
      // lists.
      auto address_entries =
          UNSAFE_BUFFERS(base::span(input_list->Address, address_count));

      // SOCKET_ADDRESS_LIST includes the first SOCKET_ADDRESS, i.e.,
      // INT iAddressCount;
      // SOCKET_ADDRESS Address[1];
      // Because there is space for `address_count` SOCKET_ADDRESSes, there is a
      // gap of sizeof(SOCKET_ADDRESS) before `sockaddr_storage_region`. This is
      // simpler than worrying about any struct alignment issues.
      size_t sockaddr_offset = sizeof(SOCKET_ADDRESS_LIST) +
                               (address_count * sizeof(SOCKET_ADDRESS));
      auto sockaddr_storage_region =
          input_buffer_storage_.as_span().subspan(sockaddr_offset);

      // Create a span of SOCKADDR_STORAGE objects in `sockaddr_storage_region`.
      // SAFETY: This is safe because `sockaddr_storage_region` was derived
      // from `input_buffer_storage_` which included space for `address_count`
      // storages.
      auto storage_span = UNSAFE_BUFFERS(base::span(
          reinterpret_cast<SOCKADDR_STORAGE*>(sockaddr_storage_region.data()),
          address_count));
      for (size_t i = 0; i < endpoints.size(); i++) {
        IPEndPoint mapped_ipe = endpoints[i];
        if (mapped_ipe.address().IsIPv4()) {
          mapped_ipe =
              IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(mapped_ipe.address()),
                         mapped_ipe.port());
        }

        struct sockaddr* addr =
            reinterpret_cast<struct sockaddr*>(&storage_span[i]);
        socklen_t addr_len = sizeof(SOCKADDR_STORAGE);

        bool result = mapped_ipe.ToSockAddr(addr, &addr_len);
        DCHECK(result);

        address_entries[i].lpSockaddr = addr;
        address_entries[i].iSockaddrLength = addr_len;
      }
    }
    ~Job() {}

    // Executed asynchronously in ThreadPool.
    void Run() {
      SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      if (sock == INVALID_SOCKET)
        return;
      DWORD result_size = 0;
      DWORD buffer_size =
          base::checked_cast<DWORD>(input_buffer_storage_.size());
      int result =
          WSAIoctl(sock, SIO_ADDRESS_LIST_SORT, input_buffer_storage_.data(),
                   buffer_size, output_buffer_storage_.data(), buffer_size,
                   &result_size, nullptr, nullptr);

      if (result == SOCKET_ERROR) {
        LOG(ERROR) << "SIO_ADDRESS_LIST_SORT failed " << WSAGetLastError();
      } else {
        success_ = true;
      }
      closesocket(sock);
    }

    // Executed on the calling thread.
    void OnComplete() {
      std::vector<IPEndPoint> sorted;
      if (success_) {
        auto* output_list = reinterpret_cast<SOCKET_ADDRESS_LIST*>(
            output_buffer_storage_.data());
        size_t address_count =
            base::checked_cast<size_t>(output_list->iAddressCount);

        // Wrap the output address array in a span for safe access.
        // SAFETY: This is safe because `output_list` was derived from
        // `output_buffer_storage_` which included space for `address_count`
        // address lists.
        auto output_entries =
            UNSAFE_BUFFERS(base::span(output_list->Address, address_count));

        sorted.reserve(address_count);
        for (const auto& entry : output_entries) {
          IPEndPoint ipe;
          bool result =
              ipe.FromSockAddr(entry.lpSockaddr, entry.iSockaddrLength);
          DCHECK(result) << "Unable to roundtrip between IPEndPoint and "
                         << "SOCKET_ADDRESS!";
          // Unmap V4MAPPED IPv6 addresses so that Happy Eyeballs works.
          if (ipe.address().IsIPv4MappedIPv6()) {
            ipe = IPEndPoint(ConvertIPv4MappedIPv6ToIPv4(ipe.address()),
                             ipe.port());
          }
          sorted.push_back(ipe);
        }
      }
      std::move(callback_).Run(success_, std::move(sorted));
    }

    CallbackType callback_;
    base::HeapArray<uint8_t> input_buffer_storage_;
    base::HeapArray<uint8_t> output_buffer_storage_;
    bool success_ = false;
  };
};

}  // namespace

// static
std::unique_ptr<AddressSorter> AddressSorter::CreateAddressSorter() {
  return std::make_unique<AddressSorterWin>();
}

}  // namespace net

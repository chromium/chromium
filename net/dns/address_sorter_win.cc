// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#include <winsock2.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/task/post_task.h"
#include "net/base/address_list.h"
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

  ~AddressSorterWin() override {}

  // AddressSorter:
  void Sort(const AddressList& list, CallbackType callback) const override {
    DCHECK(!list.empty());
    Job::Start(list, std::move(callback));
  }

 private:
  // Executes the SIO_ADDRESS_LIST_SORT ioctl asynchronously, and
  // performs the necessary conversions to/from AddressList.
  class Job : public base::RefCountedThreadSafe<Job> {
   public:
    static void Start(const AddressList& list, CallbackType callback) {
      auto job = base::WrapRefCounted(new Job(list, std::move(callback)));
      base::PostTaskAndReply(FROM_HERE,
                             {base::ThreadPool(), base::MayBlock(),
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(&Job::Run, job),
                             base::BindOnce(&Job::OnComplete, job));
    }

   private:
    friend class base::RefCountedThreadSafe<Job>;

    Job(const AddressList& list, CallbackType callback)
        : callback_(std::move(callback)),
          buffer_size_((sizeof(SOCKET_ADDRESS_LIST) +
                        base::CheckedNumeric<DWORD>(list.size()) *
                            (sizeof(SOCKET_ADDRESS) + sizeof(SOCKADDR_STORAGE)))
                           .ValueOrDie<DWORD>()),
          input_buffer_(
              reinterpret_cast<SOCKET_ADDRESS_LIST*>(malloc(buffer_size_))),
          output_buffer_(
              reinterpret_cast<SOCKET_ADDRESS_LIST*>(malloc(buffer_size_))),
          success_(false) {
      input_buffer_->iAddressCount = base::checked_cast<INT>(list.size());
      SOCKADDR_STORAGE* storage = reinterpret_cast<SOCKADDR_STORAGE*>(
          input_buffer_->Address + input_buffer_->iAddressCount);

      for (size_t i = 0; i < list.size(); ++i) {
        IPEndPoint ipe = list[i];
        // Addresses must be sockaddr_in6.
        if (ipe.address().IsIPv4()) {
          ipe = IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(ipe.address()),
                           ipe.port());
        }

        struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(storage + i);
        socklen_t addr_len = sizeof(SOCKADDR_STORAGE);
        bool result = ipe.ToSockAddr(addr, &addr_len);
        DCHECK(result);
        input_buffer_->Address[i].lpSockaddr = addr;
        input_buffer_->Address[i].iSockaddrLength = addr_len;
      }
    }

    ~Job() {}

    // Executed asynchronously in ThreadPool.
    void Run() {
      SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      if (sock == INVALID_SOCKET)
        return;
      DWORD result_size = 0;
      int result = WSAIoctl(sock, SIO_ADDRESS_LIST_SORT, input_buffer_.get(),
                            buffer_size_, output_buffer_.get(), buffer_size_,
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
      AddressList list;
      if (success_) {
        list.reserve(output_buffer_->iAddressCount);
        for (int i = 0; i < output_buffer_->iAddressCount; ++i) {
          IPEndPoint ipe;
          bool result =
              ipe.FromSockAddr(output_buffer_->Address[i].lpSockaddr,
                               output_buffer_->Address[i].iSockaddrLength);
          DCHECK(result) << "Unable to roundtrip between IPEndPoint and "
                         << "SOCKET_ADDRESS!";
          // Unmap V4MAPPED IPv6 addresses so that Happy Eyeballs works.
          if (ipe.address().IsIPv4MappedIPv6()) {
            ipe = IPEndPoint(ConvertIPv4MappedIPv6ToIPv4(ipe.address()),
                             ipe.port());
          }
          list.push_back(ipe);
        }
      }
      std::move(callback_).Run(success_, list);
    }

    CallbackType callback_;
    const DWORD buffer_size_;
    std::unique_ptr<SOCKET_ADDRESS_LIST, base::FreeDeleter> input_buffer_;
    std::unique_ptr<SOCKET_ADDRESS_LIST, base::FreeDeleter> output_buffer_;
    bool success_;

    DISALLOW_COPY_AND_ASSIGN(Job);
  };

  DISALLOW_COPY_AND_ASSIGN(AddressSorterWin);
};

}  // namespace

// static
std::unique_ptr<AddressSorter> AddressSorter::CreateAddressSorter() {
  return std::unique_ptr<AddressSorter>(new AddressSorterWin());
}

}  // namespace net

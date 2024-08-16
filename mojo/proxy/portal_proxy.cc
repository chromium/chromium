// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/proxy/portal_proxy.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/proxy/node_proxy.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "mojo/public/cpp/system/trap.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy {

using mojo::core::ScopedIpczHandle;

PortalProxy::PortalProxy(const raw_ref<const IpczAPI> ipcz,
                         NodeProxy& node_proxy,
                         ScopedIpczHandle portal,
                         mojo::ScopedMessagePipeHandle pipe)
    : ipcz_(ipcz),
      node_proxy_(node_proxy),
      portal_(std::move(portal)),
      pipe_(std::move(pipe)) {
  CHECK_EQ(mojo::CreateTrap(&OnMojoPipeActivity, &pipe_trap_), MOJO_RESULT_OK);
  const MojoResult add_trigger_result = MojoAddTrigger(
      pipe_trap_->value(), pipe_->value(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, trap_context(), nullptr);
  CHECK_EQ(add_trigger_result, MOJO_RESULT_OK);
}

PortalProxy::~PortalProxy() = default;

void PortalProxy::Start() {
  CHECK(!disconnected_);
  CHECK(!watching_portal_);
  CHECK(!watching_pipe_);

  Flush();
}

void PortalProxy::Flush() {
  CHECK(!in_flush_);
  in_flush_ = true;
  while (!disconnected_ && (!watching_portal_ || !watching_pipe_)) {
    if (!disconnected_ && !watching_portal_) {
      FlushAndWatchPortal();
    }
    if (!disconnected_ && !watching_pipe_) {
      FlushAndWatchPipe();
    }
  }
  in_flush_ = false;

  if (disconnected_) {
    // Deletes `this`.
    Die();
  }
}

void PortalProxy::FlushAndWatchPortal() {
  for (;;) {
    std::vector<uint8_t> data;
    size_t num_bytes = 0;
    std::vector<IpczHandle> handles;
    size_t num_handles = 0;
    IpczResult result =
        ipcz_->Get(portal_.get(), IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                   nullptr, &num_handles, nullptr);
    if (result == IPCZ_RESULT_OK) {
      mojo::WriteMessageRaw(pipe_.get(), nullptr, 0, nullptr, 0,
                            MOJO_WRITE_MESSAGE_FLAG_NONE);
      continue;
    }

    if (result == IPCZ_RESULT_UNAVAILABLE) {
      break;
    }

    if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
      disconnected_ = true;
      return;
    }

    data.resize(num_bytes);
    handles.resize(num_handles);
    result = ipcz_->Get(portal_.get(), IPCZ_NO_FLAGS, nullptr, data.data(),
                        &num_bytes, handles.data(), &num_handles, nullptr);
    CHECK_EQ(result, IPCZ_RESULT_OK);

    std::vector<MojoHandle> mojo_handles;
    mojo_handles.reserve(handles.size());
    for (IpczHandle handle : handles) {
      mojo_handles.push_back(TranslateIpczToMojoHandle(ScopedIpczHandle(handle))
                                 .release()
                                 .value());
    }

    mojo::WriteMessageRaw(pipe_.get(), data.data(), data.size(),
                          mojo_handles.data(), mojo_handles.size(),
                          MOJO_WRITE_MESSAGE_FLAG_NONE);
  }

  IpczTrapConditionFlags flags;
  const IpczTrapConditions trap_conditions{
      .size = sizeof(trap_conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS | IPCZ_TRAP_DEAD,
      .min_local_parcels = 0,
  };
  const IpczResult trap_result =
      ipcz_->Trap(portal_.get(), &trap_conditions, &OnIpczPortalActivity,
                  trap_context(), IPCZ_NO_FLAGS, nullptr, &flags, nullptr);
  if (trap_result == IPCZ_RESULT_OK) {
    watching_portal_ = true;
    return;
  }

  CHECK_EQ(trap_result, IPCZ_RESULT_FAILED_PRECONDITION);
  if (flags & IPCZ_TRAP_DEAD) {
    disconnected_ = true;
  }
}

void PortalProxy::FlushAndWatchPipe() {
  for (;;) {
    std::vector<uint8_t> data;
    std::vector<mojo::ScopedHandle> handles;
    const MojoResult result = mojo::ReadMessageRaw(pipe_.get(), &data, &handles,
                                                   MOJO_READ_MESSAGE_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      break;
    }

    if (result != MOJO_RESULT_OK) {
      disconnected_ = true;
      return;
    }

    std::vector<IpczHandle> ipcz_handles;
    ipcz_handles.reserve(handles.size());
    for (mojo::ScopedHandle& handle : handles) {
      ipcz_handles.push_back(
          TranslateMojoToIpczHandle(std::move(handle)).release());
    }

    const IpczResult put_result = ipcz_->Put(
        portal_.get(), data.size() ? data.data() : nullptr, data.size(),
        ipcz_handles.size() ? ipcz_handles.data() : nullptr,
        ipcz_handles.size(), IPCZ_NO_FLAGS, nullptr);
    if (put_result != IPCZ_RESULT_OK) {
      disconnected_ = true;
      return;
    }
  }

  uint32_t num_events = 1;
  MojoTrapEvent event{.struct_size = sizeof(event)};
  const MojoResult result =
      MojoArmTrap(pipe_trap_->value(), nullptr, &num_events, &event);
  if (result == MOJO_RESULT_OK) {
    watching_pipe_ = true;
    return;
  }

  CHECK_EQ(result, MOJO_RESULT_FAILED_PRECONDITION);
  CHECK_EQ(num_events, 1u);
  if (event.result == MOJO_RESULT_FAILED_PRECONDITION) {
    disconnected_ = true;
  }
}

ScopedIpczHandle PortalProxy::TranslateMojoToIpczHandle(
    mojo::ScopedHandle handle) {
  // We don't know what kind of handle is in `handle`, but we can find out.
  // First try to unwrap it as a generic platform handle.
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  const MojoResult unwrap_result =
      MojoUnwrapPlatformHandle(handle->value(), nullptr, &platform_handle);
  if (unwrap_result == MOJO_RESULT_OK) {
    std::ignore = handle.release();
    // Platform handles in ipcz are transmitted as boxed driver objects.
    return ScopedIpczHandle(
        mojo::core::ipcz_driver::WrappedPlatformHandle::MakeBoxed(
            mojo::PlatformHandle::FromMojoPlatformHandle(&platform_handle)));
  }

  // We can non-destructively probe for a shared buffer handle by calling
  // MojoGetBufferInfo().
  MojoSharedBufferInfo info = {.struct_size = sizeof(info)};
  const MojoResult info_result =
      MojoGetBufferInfo(handle->value(), nullptr, &info);
  if (info_result == MOJO_RESULT_OK) {
    auto region =
        mojo::UnwrapPlatformSharedMemoryRegion(mojo::ScopedSharedBufferHandle{
            mojo::SharedBufferHandle{handle.release().value()}});
    return ScopedIpczHandle(
        mojo::core::ipcz_driver::SharedBuffer::MakeBoxed(std::move(region)));
  }

  // Since data pipe handles are never used on Chrome OS IPC boundaries outside
  // the browser, we can assume that any other handles are message pipes.
  IpczHandle portal_to_proxy, portal_to_host;
  ipcz_->OpenPortals(mojo::core::GetIpczNode(), IPCZ_NO_FLAGS, nullptr,
                     &portal_to_proxy, &portal_to_host);
  node_proxy_->AddPortalProxy(
      ScopedIpczHandle{portal_to_proxy},
      mojo::ScopedMessagePipeHandle{
          mojo::MessagePipeHandle{handle.release().value()}});
  return ScopedIpczHandle(portal_to_host);
}

mojo::ScopedHandle PortalProxy::TranslateIpczToMojoHandle(
    ScopedIpczHandle handle) {
  // Attempt a QueryPortalStatus() call. If this succeeds, we have a portal.
  IpczPortalStatus status = {.size = sizeof(status)};
  const IpczResult query_result =
      ipcz_->QueryPortalStatus(handle.get(), IPCZ_NO_FLAGS, nullptr, &status);
  if (query_result == IPCZ_RESULT_OK) {
    // Create a new Mojo message pipe to proxy through. One end is bound to a
    // new PortalProxy with the input `handle`; the other is returned to be
    // forwarded to the legacy client.
    mojo::MessagePipe pipe;
    node_proxy_->AddPortalProxy(std::move(handle), std::move(pipe.handle0));
    return mojo::ScopedHandle{mojo::Handle{pipe.handle1.release().value()}};
  }

  // Otherwise assume it's a boxed driver object. If it's not, something has
  // gone horribly wrong, so just crash.
  auto* object = mojo::core::ipcz_driver::ObjectBase::FromBox(handle.get());
  CHECK(object);
  switch (object->type()) {
    case mojo::core::ipcz_driver::ObjectBase::Type::kWrappedPlatformHandle: {
      auto wrapped_handle =
          mojo::core::ipcz_driver::WrappedPlatformHandle::Unbox(
              handle.release());
      return mojo::WrapPlatformHandle(wrapped_handle->TakeHandle());
    }

    case mojo::core::ipcz_driver::ObjectBase::Type::kSharedBuffer: {
      auto buffer =
          mojo::core::ipcz_driver::SharedBuffer::Unbox(handle.release());
      auto mojo_buffer =
          mojo::WrapPlatformSharedMemoryRegion(std::move(buffer->region()));
      return mojo::ScopedHandle{mojo::Handle{mojo_buffer.release().value()}};
    }

    default:
      // No other types of driver objects are supported by the proxy.
      NOTREACHED();
  }
}

void PortalProxy::HandlePortalActivity(IpczTrapConditionFlags flags) {
  if (flags & IPCZ_TRAP_REMOVED) {
    // Proxy is being shut down. Do nothing.
    return;
  }

  watching_portal_ = false;
  if (flags & IPCZ_TRAP_DEAD) {
    disconnected_ = true;
    if (!in_flush_) {
      // Deletes `this`.
      Die();
      return;
    }
  } else if (!in_flush_) {
    Flush();
  }
}

void PortalProxy::HandlePipeActivity(MojoResult result) {
  if (result == MOJO_RESULT_CANCELLED) {
    // Proxy is being shut down. Do nothing.
    return;
  }

  watching_pipe_ = false;
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    disconnected_ = true;
    if (!in_flush_) {
      // Deletes `this`.
      Die();
      return;
    }
  } else if (!in_flush_) {
    Flush();
  }
}

void PortalProxy::Die() {
  CHECK(!in_flush_);
  CHECK(disconnected_);

  // Deletes `this`.
  node_proxy_->RemovePortalProxy(this);
}

}  // namespace mojo_proxy

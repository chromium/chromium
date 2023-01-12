// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"

namespace blink {

WebRtcMediaStreamTrackAdapterMap::AdapterRef::AdapterRef(
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> map,
    Type type,
    scoped_refptr<blink::WebRtcMediaStreamTrackAdapter> adapter)
    : map_(std::move(map)), type_(type), adapter_(std::move(adapter)) {
  DCHECK(map_);
  DCHECK(adapter_);
}

WebRtcMediaStreamTrackAdapterMap::AdapterRef::~AdapterRef() {
  DCHECK(map_->main_thread_->BelongsToCurrentThread());
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter> removed_adapter;
  {
    base::AutoLock scoped_lock(map_->lock_);
    // The adapter is stored in the track adapter map and we have |adapter_|,
    // so there must be at least two references to the adapter.
    DCHECK(!adapter_->HasOneRef());
    // Using a raw pointer instead of |adapter_| allows the reference count to
    // go down to one if this is the last |AdapterRef|.
    blink::WebRtcMediaStreamTrackAdapter* adapter = adapter_.get();
    adapter_ = nullptr;
    if (adapter->HasOneRef()) {
      removed_adapter = adapter;
      // "GetOrCreate..." ensures the adapter is initialized and the secondary
      // key is set before the last |AdapterRef| is destroyed. We can use either
      // the primary or secondary key for removal.
      DCHECK(adapter->is_initialized());
      if (type_ == Type::kLocal) {
        map_->local_track_adapters_.EraseByPrimary(
            adapter->track()->UniqueId());
      } else {
        map_->remote_track_adapters_.EraseByPrimary(
            adapter->webrtc_track().get());
      }
    }
  }
  // Dispose the adapter if it was removed. This is performed after releasing
  // the lock so that it is safe for any disposal mechanism to do synchronous
  // invokes to the signaling thread without any risk of deadlock.
  if (removed_adapter) {
    removed_adapter->Dispose();
  }
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::AdapterRef::Copy() const {
  base::AutoLock scoped_lock(map_->lock_);
  return base::WrapUnique(new AdapterRef(map_, type_, adapter_));
}

void WebRtcMediaStreamTrackAdapterMap::AdapterRef::InitializeOnMainThread() {
  adapter_->InitializeOnMainThread();
  if (type_ == WebRtcMediaStreamTrackAdapterMap::AdapterRef::Type::kRemote) {
    base::AutoLock scoped_lock(map_->lock_);
    if (!map_->remote_track_adapters_.FindBySecondary(track()->UniqueId())) {
      map_->remote_track_adapters_.SetSecondaryKey(webrtc_track().get(),
                                                   track()->UniqueId());
    }
  }
}

WebRtcMediaStreamTrackAdapterMap::WebRtcMediaStreamTrackAdapterMap(
    blink::PeerConnectionDependencyFactory* const factory,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread)
    : factory_(factory), main_thread_(std::move(main_thread)) {
  DCHECK(factory_);
  DCHECK(main_thread_);
}

WebRtcMediaStreamTrackAdapterMap::~WebRtcMediaStreamTrackAdapterMap() {
  DCHECK(local_track_adapters_.empty());
  DCHECK(remote_track_adapters_.empty());
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetLocalTrackAdapter(
    MediaStreamComponent* component) {
  base::AutoLock scoped_lock(lock_);
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter>* adapter_ptr =
      local_track_adapters_.FindByPrimary(component->UniqueId());
  if (!adapter_ptr)
    return nullptr;
  return base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kLocal, *adapter_ptr));
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetLocalTrackAdapter(
    webrtc::MediaStreamTrackInterface* webrtc_track) {
  base::AutoLock scoped_lock(lock_);
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter>* adapter_ptr =
      local_track_adapters_.FindBySecondary(webrtc_track);
  if (!adapter_ptr)
    return nullptr;
  return base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kLocal, *adapter_ptr));
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetOrCreateLocalTrackAdapter(
    MediaStreamComponent* component) {
  DCHECK(component);
  DCHECK(main_thread_->BelongsToCurrentThread());
  base::AutoLock scoped_lock(lock_);
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter>* adapter_ptr =
      local_track_adapters_.FindByPrimary(component->UniqueId());
  if (adapter_ptr) {
    return base::WrapUnique(
        new AdapterRef(this, AdapterRef::Type::kLocal, *adapter_ptr));
  }
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter> new_adapter;
  {
    // Do not hold |lock_| while creating the adapter in case that operation
    // synchronizes with the signaling thread. If we do and the signaling thread
    // is blocked waiting for |lock_| we end up in a deadlock.
    base::AutoUnlock scoped_unlock(lock_);
    new_adapter = blink::WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
        factory_.Lock(), main_thread_, component);
  }
  DCHECK(new_adapter->is_initialized());
  local_track_adapters_.Insert(component->UniqueId(), new_adapter);
  local_track_adapters_.SetSecondaryKey(component->UniqueId(),
                                        new_adapter->webrtc_track().get());
  return base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kLocal, new_adapter));
}

size_t WebRtcMediaStreamTrackAdapterMap::GetLocalTrackCount() const {
  base::AutoLock scoped_lock(lock_);
  return local_track_adapters_.PrimarySize();
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetRemoteTrackAdapter(
    MediaStreamComponent* component) {
  base::AutoLock scoped_lock(lock_);
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter>* adapter_ptr =
      remote_track_adapters_.FindBySecondary(component->UniqueId());
  if (!adapter_ptr)
    return nullptr;
  DCHECK((*adapter_ptr)->is_initialized());
  return base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kRemote, *adapter_ptr));
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetRemoteTrackAdapter(
    webrtc::MediaStreamTrackInterface* webrtc_track) {
  base::AutoLock scoped_lock(lock_);
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter>* adapter_ptr =
      remote_track_adapters_.FindByPrimary(webrtc_track);
  if (!adapter_ptr)
    return nullptr;
  return base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kRemote, *adapter_ptr));
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetOrCreateRemoteTrackAdapter(
    scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track) {
  DCHECK(webrtc_track);
  DCHECK(!main_thread_->BelongsToCurrentThread());
  base::AutoLock scoped_lock(lock_);
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter>* adapter_ptr =
      remote_track_adapters_.FindByPrimary(webrtc_track.get());
  if (adapter_ptr) {
    return base::WrapUnique(
        new AdapterRef(this, AdapterRef::Type::kRemote, *adapter_ptr));
  }
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter> new_adapter;
  {
    // Do not hold |lock_| while creating the adapter in case that operation
    // synchronizes with the main thread. If we do and the main thread is
    // blocked waiting for |lock_| we end up in a deadlock.
    base::AutoUnlock scoped_unlock(lock_);
    new_adapter =
        blink::WebRtcMediaStreamTrackAdapter::CreateRemoteTrackAdapter(
            factory_.Lock(), main_thread_, webrtc_track);
  }
  remote_track_adapters_.Insert(webrtc_track.get(), new_adapter);
  // The new adapter is initialized in a post to the main thread. As soon as it
  // is initialized we map its |webrtc_track| to the |remote_track_adapters_|
  // entry as its secondary key. This ensures that there is at least one
  // |AdapterRef| alive until after the adapter is initialized and its secondary
  // key is set.
  auto adapter_ref = base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kRemote, new_adapter));
  main_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterMap::AdapterRef::InitializeOnMainThread,
          std::move(adapter_ref)));
  return base::WrapUnique(
      new AdapterRef(this, AdapterRef::Type::kRemote, new_adapter));
}

size_t WebRtcMediaStreamTrackAdapterMap::GetRemoteTrackCount() const {
  base::AutoLock scoped_lock(lock_);
  return remote_track_adapters_.PrimarySize();
}

}  // namespace blink

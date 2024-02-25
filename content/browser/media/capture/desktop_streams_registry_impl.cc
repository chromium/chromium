// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_streams_registry_impl.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/random.h"

namespace {

const int kStreamIdLengthBytes = 16;

const int kApprovedStreamTimeToLiveSeconds = 10;

std::string GenerateRandomStreamId() {
  uint8_t buffer[kStreamIdLengthBytes];
  crypto::RandBytes(buffer);
  return base::Base64Encode(buffer);
}

}  // namespace

namespace content {

// static
DesktopStreamsRegistry* DesktopStreamsRegistry::GetInstance() {
  return DesktopStreamsRegistryImpl::GetInstance();
}

// static
DesktopStreamsRegistryImpl* DesktopStreamsRegistryImpl::GetInstance() {
  static base::NoDestructor<DesktopStreamsRegistryImpl> instance;
  return instance.get();
}

DesktopStreamsRegistryImpl::DesktopStreamsRegistryImpl() {}
DesktopStreamsRegistryImpl::~DesktopStreamsRegistryImpl() {}

std::string DesktopStreamsRegistryImpl::RegisterStream(
    int render_process_id,
    std::optional<int> restrict_to_render_frame_id,
    const url::Origin& origin,
    const DesktopMediaID& source,
    const DesktopStreamRegistryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string id = GenerateRandomStreamId();
  DCHECK(approved_streams_.find(id) == approved_streams_.end());
  ApprovedDesktopMediaStream& stream = approved_streams_[id];
  stream.render_process_id = render_process_id;
  stream.restrict_to_render_frame_id = restrict_to_render_frame_id;
  stream.origin = origin;
  stream.source = source;
  stream.type = type;

  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DesktopStreamsRegistryImpl::CleanupStream,
                     base::Unretained(this), id),
      base::Seconds(kApprovedStreamTimeToLiveSeconds));

  return id;
}

DesktopMediaID DesktopStreamsRegistryImpl::RequestMediaForStreamId(
    const std::string& id,
    int render_process_id,
    int render_frame_id,
    const url::Origin& origin,
    const DesktopStreamRegistryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = approved_streams_.find(id);

  // Verify that if there is a request with the specified ID it was created for
  // the same origin and the same render process and, if required, the same
  // render frame.
  if (it == approved_streams_.end() ||
      render_process_id != it->second.render_process_id ||
      (it->second.restrict_to_render_frame_id &&
       render_frame_id != it->second.restrict_to_render_frame_id) ||
      origin != it->second.origin || type != it->second.type) {
    return DesktopMediaID();
  }

  DesktopMediaID result = it->second.source;
  approved_streams_.erase(it);
  return result;
}

void DesktopStreamsRegistryImpl::CleanupStream(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  approved_streams_.erase(id);
}

}  // namespace content

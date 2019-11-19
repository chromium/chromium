// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_streams_registry_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/random.h"

namespace {

const int kStreamIdLengthBytes = 16;

const int kApprovedStreamTimeToLiveSeconds = 10;

std::string GenerateRandomStreamId() {
  char buffer[kStreamIdLengthBytes];
  crypto::RandBytes(buffer, base::size(buffer));
  std::string result;
  base::Base64Encode(base::StringPiece(buffer, base::size(buffer)), &result);
  return result;
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
    int render_frame_id,
    const url::Origin& origin,
    const DesktopMediaID& source,
    const std::string& extension_name,
    const DesktopStreamRegistryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string id = GenerateRandomStreamId();
  DCHECK(approved_streams_.find(id) == approved_streams_.end());
  ApprovedDesktopMediaStream& stream = approved_streams_[id];
  stream.render_process_id = render_process_id;
  stream.render_frame_id = render_frame_id;
  stream.origin = origin;
  stream.source = source;
  stream.extension_name = extension_name;
  stream.type = type;

  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DesktopStreamsRegistryImpl::CleanupStream,
                     base::Unretained(this), id),
      base::TimeDelta::FromSeconds(kApprovedStreamTimeToLiveSeconds));

  return id;
}

DesktopMediaID DesktopStreamsRegistryImpl::RequestMediaForStreamId(
    const std::string& id,
    int render_process_id,
    int render_frame_id,
    const url::Origin& origin,
    std::string* extension_name,
    const DesktopStreamRegistryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = approved_streams_.find(id);

  // Verify that if there is a request with the specified ID it was created for
  // the same origin and the same renderer.
  if (it == approved_streams_.end() ||
      render_process_id != it->second.render_process_id ||
      render_frame_id != it->second.render_frame_id ||
      origin != it->second.origin || type != it->second.type) {
    return DesktopMediaID();
  }

  DesktopMediaID result = it->second.source;
  if (extension_name) {
    *extension_name = it->second.extension_name;
  }
  approved_streams_.erase(it);
  return result;
}

void DesktopStreamsRegistryImpl::CleanupStream(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  approved_streams_.erase(id);
}

DesktopStreamsRegistryImpl::ApprovedDesktopMediaStream::
    ApprovedDesktopMediaStream()
    : render_process_id(-1), render_frame_id(-1) {}

}  // namespace content

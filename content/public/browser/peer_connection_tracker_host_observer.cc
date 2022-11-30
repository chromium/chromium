// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/peer_connection_tracker_host_observer.h"

#include "base/types/pass_key.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PeerConnectionTrackerHostObserver::~PeerConnectionTrackerHostObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PeerConnectionTrackerHost::RemoveObserver(
      base::PassKey<PeerConnectionTrackerHostObserver>(), this);
}

PeerConnectionTrackerHostObserver::PeerConnectionTrackerHostObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PeerConnectionTrackerHost::AddObserver(
      base::PassKey<PeerConnectionTrackerHostObserver>(), this);
}

}  // namespace content

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_SECURE_DISPLAY_LINK_TRACKER_H_
#define CONTENT_RENDERER_MEDIA_STREAM_SECURE_DISPLAY_LINK_TRACKER_H_

#include <algorithm>
#include <vector>

#include "base/stl_util.h"

// Tracks all connected links (video sinks / tracks), and reports if they are
// all secure for video capturing.
template <typename T>
class SecureDisplayLinkTracker {
 public:
  SecureDisplayLinkTracker() {}
  ~SecureDisplayLinkTracker() {}

  void Add(T* link, bool is_link_secure);
  void Remove(T* link);
  void Update(T* link, bool is_link_secure);
  bool is_capturing_secure() const { return insecure_links_.empty(); }

 private:
  // Record every insecure links.
  std::vector<T*> insecure_links_;

  DISALLOW_COPY_AND_ASSIGN(SecureDisplayLinkTracker);
};

template <typename T>
void SecureDisplayLinkTracker<T>::Add(T* link, bool is_link_secure) {
  DCHECK(!base::ContainsValue(insecure_links_, link));

  if (!is_link_secure)
    insecure_links_.push_back(link);
}

template <typename T>
void SecureDisplayLinkTracker<T>::Remove(T* link) {
  auto it = std::find(insecure_links_.begin(), insecure_links_.end(), link);
  if (it != insecure_links_.end())
    insecure_links_.erase(it);
}

template <typename T>
void SecureDisplayLinkTracker<T>::Update(T* link, bool is_link_secure) {
  auto it = std::find(insecure_links_.begin(), insecure_links_.end(), link);
  if (it != insecure_links_.end()) {
    if (is_link_secure)
      insecure_links_.erase(it);
    return;
  }
  Add(link, is_link_secure);
}

#endif  // CONTENT_RENDERER_MEDIA_STREAM_SECURE_DISPLAY_LINK_TRACKER_H_

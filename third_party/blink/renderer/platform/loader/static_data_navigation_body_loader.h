// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_STATIC_DATA_NAVIGATION_BODY_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_STATIC_DATA_NAVIGATION_BODY_LOADER_H_

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// This class allows to write navigation body from outside,
// and adheres to the contract of WebNavigationBodyLoader.
// Used for tests and static (as in "not loaded over network") response body.
class PLATFORM_EXPORT StaticDataNavigationBodyLoader
    : public WebNavigationBodyLoader {
 public:
  StaticDataNavigationBodyLoader();
  ~StaticDataNavigationBodyLoader() override;

  void Write(const char* data, size_t size);
  void Write(const SharedBuffer&);
  void Finish();

  void SetDefersLoading(bool defers) override;
  void StartLoadingBody(WebNavigationBodyLoader::Client*,
                        bool use_isolated_code_cache) override;

 private:
  void Continue();

  scoped_refptr<SharedBuffer> data_;
  WebNavigationBodyLoader::Client* client_ = nullptr;
  bool defers_loading_ = false;
  bool sent_all_data_ = false;
  bool received_all_data_ = false;
  bool is_in_continue_ = false;
  int64_t total_encoded_data_length_ = 0;
  base::WeakPtrFactory<StaticDataNavigationBodyLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_STATIC_DATA_NAVIGATION_BODY_LOADER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_CODE_CACHE_LOADER_MOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_CODE_CACHE_LOADER_MOCK_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/code_cache_loader.h"
#include "url/gurl.h"

namespace blink {

// A simple class for mocking CodeCacheLoader.
class CodeCacheLoaderMock : public CodeCacheLoader {
 public:
  CodeCacheLoaderMock() {}
  ~CodeCacheLoaderMock() override = default;

  // CodeCacheLoader methods:
  void FetchFromCodeCacheSynchronously(
      const GURL& url,
      base::Time* response_time_out,
      mojo_base::BigBuffer* buffer_out) override;
  void FetchFromCodeCache(
      blink::mojom::CodeCacheType cache_type,
      const GURL& url,
      CodeCacheLoader::FetchCodeCacheCallback callback) override;

  base::WeakPtr<CodeCacheLoaderMock> GetWeakPtr();

 private:
  base::WeakPtrFactory<CodeCacheLoaderMock> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CodeCacheLoaderMock);
};

}  // namespace blink

#endif  // CodeCacheLoaderMock_h

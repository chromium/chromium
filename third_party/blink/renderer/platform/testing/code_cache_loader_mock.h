// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_CODE_CACHE_LOADER_MOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_CODE_CACHE_LOADER_MOCK_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "url/gurl.h"

namespace blink {

// A simple class for mocking WebCodeCacheLoader.
class CodeCacheLoaderMock : public WebCodeCacheLoader {
 public:
  // A class which can be owned by both this mock loader and the creator of this
  // mock loader, which lets the creator control the behavior of the mock loader
  // without having to retain a reference to the mock loader itself.
  class Controller : public base::RefCounted<Controller> {
   public:
    void DelayResponse();
    void Respond(base::Time time, mojo_base::BigBuffer data);

   private:
    friend class CodeCacheLoaderMock;
    friend class base::RefCounted<Controller>;
    ~Controller() = default;

    // Whether to delay responses until Respond is called.
    // Otherwise responses are immediate and empty.
    bool delayed_ = false;

    // Callback saved by fetch call, if delayed_ was true.
    WebCodeCacheLoader::FetchCodeCacheCallback callback_;
  };

  explicit CodeCacheLoaderMock(scoped_refptr<Controller> controller = nullptr)
      : controller_(std::move(controller)) {}
  CodeCacheLoaderMock(const CodeCacheLoaderMock&) = delete;
  CodeCacheLoaderMock& operator=(const CodeCacheLoaderMock&) = delete;
  ~CodeCacheLoaderMock() override = default;

  // CodeCacheLoader methods:
  void FetchFromCodeCache(
      mojom::CodeCacheType cache_type,
      const WebURL& url,
      WebCodeCacheLoader::FetchCodeCacheCallback callback) override;
  void ClearCodeCacheEntry(mojom::CodeCacheType cache_type,
                           const WebURL& url) override;

 private:
  scoped_refptr<Controller> controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_CODE_CACHE_LOADER_MOCK_H_

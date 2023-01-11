// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_
#define EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/common/extension_id.h"

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace extensions {
namespace internals {

// This class is responsible for getting signed expected hashes for use in
// extension content verification.
//
// This class takes care of doing the network I/O work to ensure we
// have the contents of verified_contents.json files from the webstore.
//
// Note: This class manages its own lifetime. It deletes itself when
// Start() completes at OnSimpleLoaderComplete().
//
// Note: This class is an internal implementation detail of ContentHash and is
// not be used independently.
// TODO(lazyboy): Consider changing BUILD rules to enforce the above, yet
// keeping the class unit testable.
class ContentHashFetcher {
 public:
  // A callback for when fetch is complete.
  // The response contents is passed through std::unique_ptr<std::string>. In
  // case of failure the error code is passed as a last argument.
  using HashFetcherCallback =
      base::OnceCallback<void(ContentHash::FetchKey,
                              std::unique_ptr<std::string>,
                              ContentHash::FetchErrorCode)>;

  ContentHashFetcher(ContentHash::FetchKey fetch_key);

  ContentHashFetcher(const ContentHashFetcher&) = delete;
  ContentHashFetcher& operator=(const ContentHashFetcher&) = delete;

  // Note: |this| is deleted once OnSimpleLoaderComplete() completes.
  void Start(HashFetcherCallback hash_fetcher_callback);

 private:
  friend class base::RefCounted<ContentHashFetcher>;

  ~ContentHashFetcher();

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  ContentHash::FetchKey fetch_key_;

  HashFetcherCallback hash_fetcher_callback_;

  scoped_refptr<base::SequencedTaskRunner> response_task_runner_;

  // Alive when url fetch is ongoing.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace internals
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_

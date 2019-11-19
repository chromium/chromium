// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_JSON_FETCHER_H_
#define REMOTING_CLIENT_NOTIFICATION_JSON_FETCHER_H_

#include <string>

#include "base/callback_forward.h"

namespace base {
class Value;
}  // namespace base

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

// Interface for fetching a JSON file under https://www.gstatic.com/chromoting.
class JsonFetcher {
 public:
  using FetchJsonFileCallback =
      base::OnceCallback<void(base::Optional<base::Value>)>;

  JsonFetcher() = default;
  virtual ~JsonFetcher() = default;

  // |relative_path| is relative to https://www.gstatic.com/chromoting/.
  // Runs |done| with the decoded value if the file is successfully fetched,
  // otherwise runs |done| with base::nullopt.
  //
  // Note that the implementation MUST be able to handle concurrent requests and
  // MUST NOT keep |done| after its destructor is called.
  virtual void FetchJsonFile(
      const std::string& relative_path,
      FetchJsonFileCallback done,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_JSON_FETCHER_H_
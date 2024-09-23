// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_TMPL_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_TMPL_H_

#include "base/task/sequenced_task_runner.h"
#include "ios/web/public/session/proto/storage.pb.h"
#include "ios/web/public/web_state_id.h"

template <typename T>
void SessionRestorationService::LoadDataFromStorage(
    Browser* browser,
    base::RepeatingCallback<T(web::proto::WebStateStorage)> parse,
    base::OnceCallback<void(std::map<web::WebStateID, T>)> done) {
  // Define some types to make the code below more readable.
  using ParseCallback = base::RepeatingCallback<T(web::proto::WebStateStorage)>;
  using DoneCallback = base::OnceCallback<void(std::map<web::WebStateID, T>)>;
  using Mapping = std::map<web::WebStateID, T>;

  // The `iterator` callback invokes `parse` for each WebState storage data
  // and then store the returned value into `mapping`.
  auto mapping = std::make_unique<Mapping>();
  auto iter_callback = base::BindRepeating(
      [](Mapping* mapping, const ParseCallback& parse,
         web::WebStateID web_state_id, web::proto::WebStateStorage storage) {
        mapping->insert(
            std::make_pair(web_state_id, parse.Run(std::move(storage))));
      },
      mapping.get(), std::move(parse));

  // The `complete` callback takes ownership of `mapping` and forwards its
  // content to `done` when invoked.
  auto complete = base::BindOnce(
      [](std::unique_ptr<Mapping> mapping, DoneCallback done) {
        std::move(done).Run(std::move(*mapping));
      },
      std::move(mapping), std::move(done));

  ParseDataForBrowserAsync(browser, iter_callback, std::move(complete));
}

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_TMPL_H_

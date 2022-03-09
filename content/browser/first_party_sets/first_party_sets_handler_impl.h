// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "content/common/content_export.h"
#include "content/public/browser/first_party_sets_handler.h"

namespace content {

class CONTENT_EXPORT FirstPartySetsHandlerImpl : public FirstPartySetsHandler {
 public:
  static FirstPartySetsHandlerImpl* GetInstance();

  ~FirstPartySetsHandlerImpl() override;

  FirstPartySetsHandlerImpl(const FirstPartySetsHandlerImpl&) = delete;
  FirstPartySetsHandlerImpl& operator=(const FirstPartySetsHandlerImpl&) =
      delete;

  // FirstPartySetsHandler
  void SetPublicFirstPartySets(base::File sets_file) override;

  // This method reads the persisted First-Party Sets from the file under
  // `user_data_dir`, then invokes `send_sets` with the read data (could be
  // empty) and with a callback that should eventually be invoked with the
  // current First-Party Sets (encoded as a string). The callback writes the
  // current First-Party Sets to the file in `user_data_dir`.
  //
  // If First-Party Sets is disabled, then this method still needs to read the
  // sets, since we may still need to clear data from a previous invocation of
  // Chromium which had First-Party Sets enabled.
  void SendAndUpdatePersistedSets(
      const base::FilePath& user_data_dir,
      base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                              const std::string&)> send_sets);

 private:
  friend class base::NoDestructor<FirstPartySetsHandlerImpl>;

  FirstPartySetsHandlerImpl();

  // Called when the instance receives the "current" First-Party Sets.
  // Asynchronously writes those sets to disk.
  void OnGetUpdatedSets(const base::FilePath& path, const std::string& sets);

  // Sends `sets` via `send_sets`, and sets up a callback to overwrite the
  // on-disk sets. `send_sets` takes a callback (which is expected to be invoked
  // with the merged First-Party Sets, when ready) and the persisted sets.
  void SendPersistedSets(
      base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                              const std::string&)> send_sets,
      const base::FilePath& path,
      const std::string& sets);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_

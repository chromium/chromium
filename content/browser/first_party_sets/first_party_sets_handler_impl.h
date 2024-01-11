// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"

namespace content {

// FirstPartySetsHandlerImpl is an abstract class, encapsulating the
// content-internal details of the First-Party Sets infrastructure. This class
// is abstract so that it can be mocked during testing.
class CONTENT_EXPORT FirstPartySetsHandlerImpl : public FirstPartySetsHandler {
 public:
  static FirstPartySetsHandlerImpl* GetInstance();

  static void SetInstanceForTesting(FirstPartySetsHandlerImpl* test_instance);

  // This method reads the persisted First-Party Sets from the file under
  // `user_data_dir` and sets the First-Party Set that was provided via the
  // flag(s).
  //
  // If First-Party Sets is disabled, then this method still needs to read the
  // persisted sets, since we may still need to clear data from a previous
  // invocation of Chromium which had First-Party Sets enabled.
  //
  // Only the first call has any effect.
  void virtual Init(const base::FilePath& user_data_dir,
                    const net::LocalSetDeclaration& local_set) = 0;

  // Returns the fully-parsed and validated global First-Party Sets data.
  // Returns the data synchronously via an std::optional if it's already
  // available, or via an asynchronously-invoked callback if the data is not
  // ready yet.
  //
  // This function makes a clone of the underlying data.
  //
  // If `callback` is null, it will not be invoked, even if the First-Party Sets
  // data is not ready yet.
  //
  // If First-Party Sets is disabled, this returns a populated optional with an
  // empty GlobalFirstPartySets instance.
  [[nodiscard]] virtual std::optional<net::GlobalFirstPartySets> GetSets(
      base::OnceCallback<void(net::GlobalFirstPartySets)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_

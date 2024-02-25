// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_REDUCE_ACCEPT_LANGUAGE_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_REDUCE_ACCEPT_LANGUAGE_CONTROLLER_DELEGATE_H_

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Interface for managing reduced accept languages. It depends on existing
// language prefs service to get user's accept-language list to help the
// language negotiation process.
class CONTENT_EXPORT ReduceAcceptLanguageControllerDelegate {
 public:
  virtual ~ReduceAcceptLanguageControllerDelegate() = default;

  // Get which language was persisted for the given origin, if any.
  virtual std::optional<std::string> GetReducedLanguage(
      const url::Origin& origin) = 0;

  // Get user's current list of accepted languages.
  virtual std::vector<std::string> GetUserAcceptLanguages() const = 0;

  // Persist the language of the top-level frame for use on future visits to
  // top-level frames with the same origin.
  virtual void PersistReducedLanguage(const url::Origin& origin,
                                      const std::string& language) = 0;

  // Clear the persisted reduced language for the given origin.
  virtual void ClearReducedLanguage(const url::Origin& origin) = 0;
};

}  // namespace content
#endif  // CONTENT_PUBLIC_BROWSER_REDUCE_ACCEPT_LANGUAGE_CONTROLLER_DELEGATE_H_

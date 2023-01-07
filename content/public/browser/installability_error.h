// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INSTALLABILITY_ERROR_H_
#define CONTENT_PUBLIC_BROWSER_INSTALLABILITY_ERROR_H_

#include <ostream>
#include <string>
#include <vector>

#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT InstallabilityErrorArgument {
  InstallabilityErrorArgument() = default;
  InstallabilityErrorArgument(std::string name, std::string value);
  bool operator==(const InstallabilityErrorArgument& other) const;
  ~InstallabilityErrorArgument();

  std::string name;
  std::string value;
};

struct CONTENT_EXPORT InstallabilityError {
  InstallabilityError();
  explicit InstallabilityError(std::string error_id);
  InstallabilityError(const InstallabilityError& other);
  InstallabilityError(InstallabilityError&& other);
  bool operator==(const InstallabilityError& other) const;
  ~InstallabilityError();

  std::string error_id;
  std::vector<InstallabilityErrorArgument> installability_error_arguments;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& os,
                                        const InstallabilityError& error);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INSTALLABILITY_ERROR_H_

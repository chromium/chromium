// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/manage_storage_url_util.h"

#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

// URL template to redirect a user to the URL given as the `continue` query
// parameter, using the Google account given as the `Email` query parameter.
// This way, if the URL given as the `continue` query parameter is
// `kManageStorageURL`, the user will be redirected to manage the storage
// of the account associated with the given email.
constexpr char kAccountChooserRedirectionUrlTemplate[] =
    "https://accounts.google.com/AccountChooser";
constexpr char kAccountChooserRedirectionUrlEmailParameterName[] = "Email";
constexpr char kAccountChooserRedirectionUrlContinueParameterName[] =
    "continue";
// URL to let the user manage their Drive storage.
constexpr char kManageStorageURL[] = "https://one.google.com/storage";

}  // namespace

GURL GenerateManageDriveStorageUrl(std::string_view user_email) {
  // The user email is used to switch to the appropriate account before loading
  // the "Manage Storage" web page.
  GURL result(kAccountChooserRedirectionUrlTemplate);
  // Set 'Email' query parameter.
  result = net::AppendQueryParameter(
      result, kAccountChooserRedirectionUrlEmailParameterName, user_email);
  // Set 'continue' query parameter.
  result = net::AppendQueryParameter(
      result, kAccountChooserRedirectionUrlContinueParameterName,
      kManageStorageURL);
  return result;
}

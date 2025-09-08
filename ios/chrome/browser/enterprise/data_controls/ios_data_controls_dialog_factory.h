// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_DATA_CONTROLS_DIALOG_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_DATA_CONTROLS_DIALOG_FACTORY_H_

#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "components/enterprise/data_controls/core/browser/data_controls_dialog.h"
#import "ios/chrome/browser/enterprise/data_controls/ios_data_controls_dialog.h"

namespace web {
class WebState;
}  // namespace web

namespace data_controls {

// Factory interface to create `IOSDataControlsDialog`s.
//
// TODO(crbug.com/352728209): Refactor the
// chrome/browser/enterprise/data_controls/data_controls_dialog_factory.cc, so
// this factory class can inherit it and remove the deuped code.
class IOSDataControlsDialogFactory {
 public:
  static IOSDataControlsDialogFactory* GetInstance();

  IOSDataControlsDialogFactory(const IOSDataControlsDialogFactory&) = delete;
  IOSDataControlsDialogFactory& operator=(const IOSDataControlsDialogFactory&) =
      delete;

  // Entry point to be used to show a `IOSDataControlsDialog`. It's possible no
  // extra dialog will be shown in certain cases, for example if one already
  // exists for `type` in the current context.
  void ShowDialogIfNeeded(web::WebState* web_state,
                          DataControlsDialog::Type type);

  void ShowDialogIfNeeded(web::WebState* web_state,
                          DataControlsDialog::Type type,
                          base::OnceCallback<void(bool bypassed)> callback);

 private:
  friend class base::NoDestructor<IOSDataControlsDialogFactory>;

  IOSDataControlsDialogFactory();
  ~IOSDataControlsDialogFactory();

  IOSDataControlsDialog* CreateDialog(
      DataControlsDialog::Type type,
      web::WebState* web_state,
      base::OnceCallback<void(bool bypassed)> callback);
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_DATA_CONTROLS_DIALOG_FACTORY_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"

#import <iterator>

#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/outgoing_tab_form_field_extractor.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/origin.h"

namespace send_tab_to_self {

PageContext ExtractFormFieldsFromWebState(web::WebState* web_state) {
  if (!web_state) {
    return PageContext();
  }

  const url::Origin main_origin =
      url::Origin::Create(web_state->GetLastCommittedURL());

  PageContext context;

  web::WebFramesManager* frames_manager =
      autofill::GetWebFramesManagerForAutofill(web_state);

  for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
    autofill::AutofillDriverIOS* driver =
        autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state, frame);
    if (!driver) {
      continue;
    }

    PageContext::FormFieldInfo frame_info =
        ExtractOutgoingTabFormFields(driver->GetAutofillManager(), main_origin);
    context.form_field_info.fields.insert(
        context.form_field_info.fields.end(),
        std::make_move_iterator(frame_info.fields.begin()),
        std::make_move_iterator(frame_info.fields.end()));
  }

  return context;
}

}  // namespace send_tab_to_self

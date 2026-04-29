// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"

#import <iterator>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/outgoing_tab_form_field_extractor.h"
#import "components/send_tab_to_self/received_tab_forms_filler.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/origin.h"

namespace send_tab_to_self {

OpenNewTabCommand* CreateOpenNewTabCommand(const SendTabToSelfEntry* entry) {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:entry->GetURL()];
  command.sendTabToSelfEntryGUID = base::SysUTF8ToNSString(entry->GetGUID());

  const send_tab_to_self::ScrollPosition& scroll_position =
      entry->GetPageContext().scroll_position;
  if (base::FeatureList::IsEnabled(kSendTabToSelfPropagateScrollPosition) &&
      !scroll_position.IsEmpty()) {
    shared_highlighting::TextFragment fragment(
        scroll_position.text_fragment.text_start,
        scroll_position.text_fragment.text_end,
        scroll_position.text_fragment.prefix,
        scroll_position.text_fragment.suffix);
    command.textFragment = base::SysUTF8ToNSString(fragment.ToEscapedString(
        shared_highlighting::TextFragment::EscapedStringFormat::
            kWithoutTextDirective));
  }

  return command;
}

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

void FillWebState(web::WebState* web_state,
                  const url::Origin& origin,
                  const PageContext& page_context) {
  CHECK(web_state);

  // Return early if there is no form data to fill.
  if (page_context.form_field_info.fields.empty()) {
    return;
  }

  autofill::AutofillClientIOS* autofill_client =
      autofill::AutofillClientIOS::FromWebState(web_state);
  if (autofill_client) {
    ReceivedTabFormsFiller::Start(*autofill_client, origin,
                                  page_context.form_field_info);
  }
}

}  // namespace send_tab_to_self

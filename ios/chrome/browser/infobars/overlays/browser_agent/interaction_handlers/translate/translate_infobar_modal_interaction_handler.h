// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_MODAL_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_MODAL_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"

namespace translate {
class TranslateInfoBarDelegate;
}

// Helper object that updates the model layer for interaction events with the
// Translate infobar modal UI.
class TranslateInfobarModalInteractionHandler
    : public InfobarModalInteractionHandler {
 public:
  TranslateInfobarModalInteractionHandler();
  ~TranslateInfobarModalInteractionHandler() override;

  // Instructs the handler that the user has used `infobar`'s modal UI to
  // request that the always translate preference be toggled.
  virtual void ToggleAlwaysTranslate(InfoBarIOS* infobar);
  // Instructs the handler that the user has used `infobar`'s modal UI to
  // request that the never translate source language preference be toggled.
  virtual void ToggleNeverTranslateLanguage(InfoBarIOS* infobar);
  // Instructs the handler that the user has used `infobar`'s modal UI to
  // request that the never translate site preference be toggled.
  virtual void ToggleNeverTranslateSite(InfoBarIOS* infobar);
  // Instructs the handler that the user has used `infobar`'s modal UI to
  // request that the translation be reverted.
  virtual void RevertTranslation(InfoBarIOS* infobar);
  // Instructs the handler that the user has used `infobar`'s modal UI to
  // request that the source language change to the language at
  // `source_language_index` and/or the target language change to the language
  // at `target_language_index`. If either do not need to be updated, then the
  // index passed should be -1.
  virtual void UpdateLanguages(InfoBarIOS* infobar,
                               int source_language_index,
                               int target_language_index);

  // InfobarModalInteractionHandler:
  void PerformMainAction(InfoBarIOS* infobar) override;

  // InfobarInteractionHandler::Handler:
  void InfobarVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

 private:
  // Initiates a translate for `infobar`.
  void StartTranslation(InfoBarIOS* infobar);

  // InfobarModalInteractionHandler:
  std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
  CreateModalInstaller() override;

  // Returns the translate delegate from `infobar`.
  translate::TranslateInfoBarDelegate* GetDelegate(InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_MODAL_INTERACTION_HANDLER_H_

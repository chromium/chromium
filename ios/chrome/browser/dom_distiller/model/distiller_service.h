// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_H_

#import <memory>

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/core/distiller.h"
#import "components/dom_distiller/core/distiller_page.h"
#import "components/keyed_service/core/keyed_service.h"
#import "url/gurl.h"

class PrefService;

// Profile-keyed service that provides access to attributes related to
// web page content distillation.
class DistillerService : public KeyedService {
 public:
  DistillerService(
      std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory,
      PrefService* pref_service);
  ~DistillerService() override;

  // Extracts the content from the given `url` web page with instructions
  // from the `distiller_page` and runs the callback with a structured
  // result containing the simplified html, page, and image information.
  void DistillPage(
      const GURL& url,
      std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
      dom_distiller::Distiller::DistillationFinishedCallback finished_cb,
      const dom_distiller::Distiller::DistillationUpdateCallback& update_cb);

  // Returns the cross-platform customization preferences for viewing
  // distillation output.
  dom_distiller::DistilledPagePrefs* GetDistilledPagePrefs();

  // KeyedService implementation.
  void Shutdown() override;

 private:
  std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory_;
  std::unique_ptr<dom_distiller::Distiller> distiller_;
  std::unique_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs_;
};

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_H_

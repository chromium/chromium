// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_H_

#import <deque>
#import <memory>

#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
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
  // Used to queue distillation requests received while processing another
  // request (i.e. when `distiller_` is non null).
  struct Request {
    GURL url;
    std::unique_ptr<dom_distiller::DistillerPage> distiller_page;
    dom_distiller::Distiller::DistillationFinishedCallback finished_cb;
    dom_distiller::Distiller::DistillationUpdateCallback update_cb;

    Request(GURL url,
            std::unique_ptr<dom_distiller::DistillerPage> distiller_page,
            dom_distiller::Distiller::DistillationFinishedCallback finished_cb,
            dom_distiller::Distiller::DistillationUpdateCallback update_cb);

    Request(Request&&);
    Request& operator=(Request&&);

    ~Request();
  };

  // Called when a distillation request completes.
  void DistillationFinished();

  // Called to start the next pending distillation request.
  void ProcessNextDistillation();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<dom_distiller::DistillerFactory> distiller_factory_;
  std::unique_ptr<dom_distiller::DistilledPagePrefs> distilled_page_prefs_;

  // The distiller is created when processing a distillation request, and
  // then destroyed when the request has completed. If another request is
  // received while the distiller is non-null, then it is queued.
  std::unique_ptr<dom_distiller::Distiller> distiller_;
  std::deque<Request> pending_requests_;

  base::WeakPtrFactory<DistillerService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_H_

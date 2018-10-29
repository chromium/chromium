// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SCHEDULER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SCHEDULER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

class BackgroundFetchRegistrationId;
class BackgroundFetchRequestInfo;

// Maintains a list of Controllers and chooses which ones should launch new
// downloads.
class CONTENT_EXPORT BackgroundFetchScheduler {
 public:
  using FinishedCallback =
      base::OnceCallback<void(const BackgroundFetchRegistrationId&,
                              blink::mojom::BackgroundFetchFailureReason)>;

  // Interface for download job controllers.
  class CONTENT_EXPORT Controller {
   public:
    using RequestFinishedCallback =
        base::OnceCallback<void(scoped_refptr<BackgroundFetchRequestInfo>)>;

    virtual ~Controller();

    // Returns whether the Controller has any pending download requests.
    virtual bool HasMoreRequests() = 0;

    // Requests the download manager to start fetching |request|.
    virtual void StartRequest(scoped_refptr<BackgroundFetchRequestInfo> request,
                              RequestFinishedCallback callback) = 0;

    // Returns a list of requests that started in a previous session and did not
    // complete. Clears the list of outstanding GUIDs in the controller.
    virtual std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
    TakeOutstandingRequests() = 0;

    void Finish(blink::mojom::BackgroundFetchFailureReason reason_to_abort);

    const BackgroundFetchRegistrationId& registration_id() const {
      return registration_id_;
    }

   protected:
    Controller(BackgroundFetchScheduler* scheduler,
               const BackgroundFetchRegistrationId& registration_id,
               FinishedCallback finished_callback);

   private:
    // The |scheduler_| is guaranteed to outlive the controllers due to their
    // declaration order in the BackgroundFetchContext.
    BackgroundFetchScheduler* scheduler_;

    BackgroundFetchRegistrationId registration_id_;
    FinishedCallback finished_callback_;
  };

  using MarkRequestCompleteCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;
  using NextRequestCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              scoped_refptr<BackgroundFetchRequestInfo>)>;

  class CONTENT_EXPORT RequestProvider {
   public:
    virtual ~RequestProvider() {}

    // Retrieves the next pending request for |registration_id| and invoke
    // |callback| with it.
    virtual void PopNextRequest(
        const BackgroundFetchRegistrationId& registration_id,
        NextRequestCallback callback) = 0;

    // Marks |request_info| as complete and calls |callback| when done.
    virtual void MarkRequestAsComplete(
        const BackgroundFetchRegistrationId& registration_id,
        scoped_refptr<BackgroundFetchRequestInfo> request_info,
        MarkRequestCompleteCallback callback) = 0;
  };

  explicit BackgroundFetchScheduler(RequestProvider* request_provider);
  ~BackgroundFetchScheduler();

  // Adds a new job controller to the scheduler. May immediately start to
  // schedule jobs for |controller|.
  void AddJobController(Controller* controller);

  // Removes the |controller| from the scheduler. Pending updates will be
  // ignored and it won't be considered for further requests.
  void RemoveJobController(Controller* controller);

 private:
  void ScheduleDownload();

  void DidPopNextRequest(
      blink::mojom::BackgroundFetchError error,
      scoped_refptr<BackgroundFetchRequestInfo> request_info);

  void MarkRequestAsComplete(
      scoped_refptr<BackgroundFetchRequestInfo> request_info);
  void DidMarkRequestAsComplete(blink::mojom::BackgroundFetchError error);

  RequestProvider* request_provider_;

  // The scheduler owns all the job controllers, holding them either in the
  // controller queue or the guid to controller map.
  base::circular_deque<Controller*> controller_queue_;
  Controller* active_controller_ = nullptr;

  base::WeakPtrFactory<BackgroundFetchScheduler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SCHEDULER_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_FIND_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_FIND_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
class WebViewInternalFindFunction;
class WebViewGuest;

// Helper class for find requests and replies for the web_view_internal find
// API.
class WebViewFindHelper {
 public:
  explicit WebViewFindHelper(WebViewGuest* webview_guest);

  WebViewFindHelper(const WebViewFindHelper&) = delete;
  WebViewFindHelper& operator=(const WebViewFindHelper&) = delete;

  ~WebViewFindHelper();

  // Cancels all find requests in progress and calls their callback functions.
  void CancelAllFindSessions();

  // Dispatches the |findupdate| event.
  void DispatchFindUpdateEvent(bool canceled, bool final_update);

  // Ends the find session with id |session_request_id|  and calls the
  // appropriate callbacks.
  void EndFindSession(int session_request_id, bool canceled);

  // Helper function for WebViewGuest::Find().
  void Find(content::WebContents* guest_web_contents,
            const std::u16string& search_text,
            blink::mojom::FindOptionsPtr options,
            scoped_refptr<WebViewInternalFindFunction> find_function);

  // Helper function for WeViewGuest:FindReply().
  void FindReply(int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update);

 private:
  // A wrapper to store find results.
  class FindResults {
   public:
    FindResults();

    FindResults(const FindResults&) = delete;
    FindResults& operator=(const FindResults&) = delete;

    ~FindResults();

    // Aggregate the find results.
    void AggregateResults(int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update);

    // Stores find results into a base::Value::Dict.
    void PrepareResults(base::Value::Dict& results);

   private:
    int number_of_matches_;
    int active_match_ordinal_;
    gfx::Rect selection_rect_;

    friend void WebViewFindHelper::EndFindSession(int session_request_id,
                                                  bool canceled);
  };

  // Stores and processes the results for the |findupdate| event.
  class FindUpdateEvent {
   public:
    explicit FindUpdateEvent(const std::u16string& search_text);

    FindUpdateEvent(const FindUpdateEvent&) = delete;
    FindUpdateEvent& operator=(const FindUpdateEvent&) = delete;

    ~FindUpdateEvent();

    // Aggregate the find results.
    void AggregateResults(int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update);

    // Stores find results and other event info into a Value::Dict.
    void PrepareResults(base::Value::Dict& dict);

   private:
    const std::u16string search_text_;
    FindResults find_results_;
  };

  // Handles all information about a find request and its results.
  class FindInfo : public base::RefCounted<FindInfo> {
   public:
    FindInfo(int request_id,
             const std::u16string& search_text,
             blink::mojom::FindOptionsPtr options,
             scoped_refptr<WebViewInternalFindFunction> find_function);

    FindInfo(const FindInfo&) = delete;
    FindInfo& operator=(const FindInfo&) = delete;

    // Add another request to |find_next_requests_|.
    void AddFindNextRequest(const base::WeakPtr<FindInfo>& request) {
      find_next_requests_.push_back(request);
    }

    // Aggregate the find results.
    void AggregateResults(int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update);

    base::WeakPtr<FindInfo> AsWeakPtr();

    const blink::mojom::FindOptionsPtr& options() { return options_; }

    bool replied() {
      return replied_;
    }

    int request_id() {
      return request_id_;
    }

    const std::u16string& search_text() { return search_text_; }

    // Calls the callback function within |find_function_| with the find results
    // from within |find_results_|.
    void SendResponse(bool canceled);

   private:
    friend class base::RefCounted<FindInfo>;

    ~FindInfo();

    const int request_id_;
    const std::u16string search_text_;
    blink::mojom::FindOptionsPtr options_;
    scoped_refptr<WebViewInternalFindFunction> find_function_;
    FindResults find_results_;

    // A find reply has been received for this find request.
    bool replied_;

    // Stores pointers to all the find next requests if this is the first
    // request of a find session.
    std::vector<base::WeakPtr<FindInfo> > find_next_requests_;

    friend void WebViewFindHelper::EndFindSession(int session_request_id,
                                                  bool canceled);

    base::WeakPtrFactory<FindInfo> weak_ptr_factory_{this};
  };

  // Pointer to the webview that is being helped.
  const raw_ptr<WebViewGuest> webview_guest_;

  // A counter to generate a unique request id for a find request.
  // We only need the ids to be unique for a given WebViewGuest.
  int current_find_request_id_;

  // Stores aggregated find results and other info for the |findupdate| event.
  std::unique_ptr<FindUpdateEvent> find_update_event_;

  // Pointer to the first request of the current find session. find_info_map_
  // retains ownership.
  scoped_refptr<FindInfo> current_find_session_;

  // Stores each find request's information by request_id so that its callback
  // function can be called when its find results are available.
  using FindInfoMap = std::map<int, scoped_refptr<FindInfo>>;
  FindInfoMap find_info_map_;
};

} // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_FIND_HELPER_H_

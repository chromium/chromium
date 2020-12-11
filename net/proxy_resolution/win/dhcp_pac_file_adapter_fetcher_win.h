// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_DHCP_PAC_FILE_ADAPTER_FETCHER_WIN_H_
#define NET_PROXY_RESOLUTION_WIN_DHCP_PAC_FILE_ADAPTER_FETCHER_WIN_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class TaskRunner;
}

namespace net {

class PacFileFetcher;
class URLRequestContext;

// For a given adapter, this class takes care of first doing a DHCP lookup
// to get the PAC URL, then if there is one, trying to fetch it.
class NET_EXPORT_PRIVATE DhcpPacFileAdapterFetcher
    : public base::SupportsWeakPtr<DhcpPacFileAdapterFetcher> {
 public:
  // |url_request_context| must outlive DhcpPacFileAdapterFetcher.
  // |task_runner| will be used to post tasks to a thread.
  DhcpPacFileAdapterFetcher(URLRequestContext* url_request_context,
                            scoped_refptr<base::TaskRunner> task_runner);
  virtual ~DhcpPacFileAdapterFetcher();

  // Starts a fetch.  On completion (but not cancellation), |callback|
  // will be invoked with the network error indicating success or failure
  // of fetching a DHCP-configured PAC file on this adapter.
  //
  // On completion, results can be obtained via |GetPacScript()|, |GetPacURL()|.
  //
  // You may only call Fetch() once on a given instance of
  // DhcpPacFileAdapterFetcher.
  virtual void Fetch(const std::string& adapter_name,
                     CompletionOnceCallback callback,
                     const NetworkTrafficAnnotationTag traffic_annotation);

  // Cancels the fetch on this adapter.
  virtual void Cancel();

  // Returns true if in the FINISH state (not CANCEL).
  virtual bool DidFinish() const;

  // Returns the network error indicating the result of the fetch. Will
  // return IO_PENDING until the fetch is complete or cancelled. This is
  // the same network error passed to the |callback| provided to |Fetch()|.
  virtual int GetResult() const;

  // Returns the contents of the PAC file retrieved.  Only valid if
  // |IsComplete()| is true.  Returns the empty string if |GetResult()|
  // returns anything other than OK.
  virtual base::string16 GetPacScript() const;

  // Returns the PAC URL retrieved from DHCP.  Only guaranteed to be
  // valid if |IsComplete()| is true.  Returns an empty URL if no URL was
  // configured in DHCP.  May return a valid URL even if |result()| does
  // not return OK (this would indicate that we found a URL configured in
  // DHCP but failed to download it).
  virtual GURL GetPacURL() const;

  // Returns the PAC URL configured in DHCP for the given |adapter_name|, or
  // the empty string if none is configured.
  //
  // This function executes synchronously due to limitations of the Windows
  // DHCP client API.
  static std::string GetPacURLFromDhcp(const std::string& adapter_name);

  // Sanitizes a string returned via the DHCP API.
  static std::string SanitizeDhcpApiString(const char* data,
                                           size_t count_bytes);

 protected:
  // This is the state machine for fetching from a given adapter.
  //
  // The state machine goes from START->WAIT_DHCP when it starts
  // a worker thread to fetch the PAC URL from DHCP.
  //
  // In state WAIT_DHCP, if the DHCP query finishes and has no URL, it
  // moves to state FINISH.  If there is a URL, it starts a
  // PacFileFetcher to fetch it and moves to state WAIT_URL.
  //
  // It goes from WAIT_URL->FINISH when the PacFileFetcher completes.
  //
  // In state FINISH, completion is indicated to the outer class, with
  // the results of the fetch if a PAC script was successfully fetched.
  //
  // In state WAIT_DHCP, our timeout occurring can push us to FINISH.
  //
  // In any state except FINISH, a call to Cancel() will move to state
  // CANCEL and cause all outstanding work to be cancelled or its
  // results ignored when available.
  enum State {
    STATE_START,
    STATE_WAIT_DHCP,
    STATE_WAIT_URL,
    STATE_FINISH,
    STATE_CANCEL,
  };

  State state() const;

  // This inner class encapsulates work done on a worker pool thread.
  // By using a separate object, we can keep the main object completely
  // thread safe and let it be non-refcounted.
  class NET_EXPORT_PRIVATE DhcpQuery
      : public base::RefCountedThreadSafe<DhcpQuery> {
   public:
    DhcpQuery();

    // This method should run on a worker pool thread, via PostTaskAndReply.
    // After it has run, the |url()| method on this object will return the
    // URL retrieved.
    void GetPacURLForAdapter(const std::string& adapter_name);

    // Returns the URL retrieved for the given adapter, once the task has run.
    const std::string& url() const;

   protected:
    // Virtual method introduced to allow unit testing.
    virtual std::string ImplGetPacURLFromDhcp(const std::string& adapter_name);

    friend class base::RefCountedThreadSafe<DhcpQuery>;
    virtual ~DhcpQuery();

   private:
    // The URL retrieved for the given adapter.
    std::string url_;

    DISALLOW_COPY_AND_ASSIGN(DhcpQuery);
  };

  // Virtual methods introduced to allow unit testing.
  virtual std::unique_ptr<PacFileFetcher> ImplCreateScriptFetcher();
  virtual DhcpQuery* ImplCreateDhcpQuery();
  virtual base::TimeDelta ImplGetTimeout() const;

 private:
  // Event/state transition handlers
  void OnDhcpQueryDone(scoped_refptr<DhcpQuery> dhcp_query,
                       const NetworkTrafficAnnotationTag traffic_annotation);
  void OnTimeout();
  void OnFetcherDone(int result);
  void TransitionToFinish();

  // TaskRunner for posting tasks to a worker thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  // Current state of this state machine.
  State state_;

  // A network error indicating result of operation.
  int result_;

  // Empty string or the PAC script downloaded.
  base::string16 pac_script_;

  // Empty URL or the PAC URL configured in DHCP.
  GURL pac_url_;

  // Callback to let our client know we're done. Invalid in states
  // START, FINISH and CANCEL.
  CompletionOnceCallback callback_;

  // Fetcher to retrieve PAC files once URL is known.
  std::unique_ptr<PacFileFetcher> script_fetcher_;

  // Implements a timeout on the call to the Win32 DHCP API.
  base::OneShotTimer wait_timer_;

  URLRequestContext* const url_request_context_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DhcpPacFileAdapterFetcher);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_DHCP_PAC_FILE_ADAPTER_FETCHER_WIN_H_

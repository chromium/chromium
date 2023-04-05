# OverlayPresenter

OverlayPresenter is used to schedule the display of UI alongside the content
area of a WebState.

## Classes of note:

##### OverlayRequest

OverlayRequests are model objects that are used to schedule the display of
UI alongside a WebState's content area.  They are created with an
OverlayUserData subclass containing the information necessary to configure the
requested UI.

##### OverlayResponse

OverlayResponses are provided to each OverlayRequest to describe the user's
interaction with the overlay UI.  Clients should create OverlayResponses with
OverlayUserData subclasses with the overlay UI user interaction information
necessary to execute the callback for that overlay.

##### OverlayCallbackManager

Each OverlayRequest owns an OverlayCallbackManager, which is used to communicate
user interaction events from the overlay UI to the requesting site.  It supports
two types of callbacks: completion callbacks and dispatch callbacks.  Completion
callbacks are executed when the overlay UI is dismissed or the OverlayRequest is
cancelled.  These callbacks are executed with a completion OverlayResponse.  In
order to handle model updates for user interaction events for ongoing overlay
UI, the callback manager also supports callbacks for OverlayResponses dispatched
before overlay completion.  OverlayPresenter clients can register callbacks to
be executed upon every dispatch of an OverlayResponse created with a specific
type of response info type.

##### OverlayRequestQueue

Each WebState has an OverlayRequestQueue at each OverlayModality that stores the
OverlayRequests for overlays to be displayed alongside that WebState's content
area.  When a client wishes to schedule the display of an overlay, it should
add an OverlayRequest to the desired WebState's queue.  This will trigger the
scheduling logic for that request's corresponding overlay UI.

##### OverlayPresenter

OverlayPresenter drives the presentation of the UI for OverlayRequests added to
queues for WebStates in a Browser.

##### OverlayPresentationContext

Clients must provide a presentation context to a Browser's OverlayPresenter that
handles the presentation of overlay UI for that presenter's modality and
Browser.

##### OverlayPresenterObserver

Objects that care about the presentation and dismissal of overlay UI by the
presenter should add themselves as observers to the presenter.  This can be used
to respond to update UI for UI presentation, for example to update the location
bar text while a dialog is displayed.  Additionally, observers can use these
hook points to add additional callbacks to the request.

## Setting up OverlayPresenter:

Multiple OverlayPresenters may be active for a single Browser to manage overlay
UI at different levels of modality (i.e. modal over WebState content area, modal
over entire browser, etc).

Each instance of OverlayPresenter must be provided with an OverlayPresenter::
UIDelegate that manages the overlay UI at the modality associated with the
presenter.

## Example usage of presenter:

### Showing an alert with a title, message, an OK button, and a Cancel button

##### 1. Create OverlayUserData subclasses for the requests and responses:

A request configuration user data should be created with the information
necessary to set up the overlay UI being requested.

    class AlertConfig : public OverlayUserData<AlertConfig> {
     public:
      const std::string& title() const;
      const std::string& message() const;
      const std::vector<std::string>& button_titles() const;
     private:
      OVERLAY_USER_DATA_SETUP(AlertConfig);
      AlertConfig(const std::string& title, const std::string& message);
    };

A response ino user data should be created with the information necessary to
execute the callback for the overlay.

    class AlertInfo : public OverlayUserData<AlertInfo> {
     public:
      const size_t tapped_button_index() const;
     private:
      OVERLAY_USER_DATA_SETUP(AlertInfo);
      AlertInfo(size_t tapped_button_index);
    };

##### 2. Request an overlay using the request config user data.

An OverlayRequest for the alert can be created using:

    OverlayRequest::CreateWithConfig<AlertConfig>(
        "alert title", "message text");

A callback can be added to the request to use the response info:

    OverlayCompletionCallback callback =
        base::BindOnce(^(OverlayResponse* response) {
      if (!response)
        return;
      AlertInfo* info = response->GetInfo<AlertInfo>();
      /* Handle button tap at info->tapped_button_index() */
    });
    request->GetCallbackManager()->AddCompletionCallback(std::move(callback));

Clients can then supply this request to the OverlayRequestQueue corresponding
with the WebState alongside which the overlay should be shown:

    OverlayModality modality =
        OverlayModality::kWebContentArea;
    OverlayRequestQueue::FromWebState(web_state, modality)->
        AddRequest(std::move(request));

##### 3. Supply a response to the request.

Upon the user tapping a button on the alert, say at index 0, a response can be
created and supplied to that request.

    OverlayRequestQueue::FromWebState(web_state, modality)
        ->front_request()
        ->GetCallbackManager()
        ->SetCompletionResponse(OverlayResponse::CreateWithInfo<AlertInfo>(0));

### Dispatching responses for ongoing overlays

This section documents how to update model objects for user interaction events
in overlay UI that has not been dismissed yet.  As an example situation, let's
say that there's a non-modal account picking screen.  When the user taps on an
account, we want to update the username text in a separate view without
dismissing the overlay UI.  This can be accomplished by dispatching responses
through the callback manager.

##### 1. Create OverlayUserData subclasses for the dispatched response:

OverlayRequests configured with AccountChooserUserInfos will be dispatched via
the OverlayRequest's callback manager in order to update the username label in
a browser view.

    class AccountChooserUserInfo :
        public OverlayUserData<AccountChooserUserInfo> {
     public:
      const std::string& username() const;
     private:
      OVERLAY_USER_DATA_SETUP(AccountChooserUserInfo);
      AccountChooserUserInfo(const std::string& username);
    };

##### 2. Use OverlayPresenterObserver to add a dispatch callback:

The mediator for the UI showing the username label should register itself as an
OverlayPresenterObserver so that it can be aware of the presentation of UI.

    @interface UsernameLabelMediator ()<OverlayPresenterObserving>
    @end

    @implementation UsernameLabelMediator

    - (void)overlayPresenter:(OverlayPresenter\*)presenter
        willShowOverlayForRequest:(OverlayRequest*)request {
      // Only add the dispatch callback for requests created with the desired
      // config.  This mediator only cares about requests for the account
      // chooser UI (AccountChooserRequestConfig definition not shown).
      if (!request->GetConfig<AccountChooserRequestConfig>())
        return;
      __weak __typeof__(self) weakSelf = self;
      OverlayDispatchCallback callback =
          base::BindRepeating(^(OverlayResponse* response) {
        weakSelf.consumer.usernameText = base::SysUTF8ToNSString(
            response->GetInfo<AccountChooserUserInfo>()->username());
      });
      request->GetCallbackManager()->AddDispatchCallback(std::move(callback));
    }

    @end

##### 3. Dispatch OverlayResponses for non-dismissing UI events

When the user taps on a row in the account chooser overlay UI, the overlay's
mediator would handle this event by dispatching an OverlayRequest created with
an AccountChooserUserInfo in order to trigger callbacks for that type of
response.

    @implementation AccountChooserOverlayMediator

    - (void)didTapOnAccountWithUsername:(NSString\*)username {
      self.request->GetCallbackManager()->DispatchResponse(
          OverlayResponse::CreateWithInfo<AccountChooserUserInfo>(
          base::SysNSStringToUTF8(username)));
    }

    @end

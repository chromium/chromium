# Chrome/Payments client-side flows

**Last updated (including links) January 2018**

## Chrome Downstream (card unmasking)

1.  A credit card is selected in the Autofill suggestions dropdown, [which
    triggers](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=777&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    AutofillManager::FillOrPreviewForm, which triggers
    AutofillManager::FillOrPreviewCreditCardForm
2.  If the form is [being filled with a
    MASKED_SERVER_CARD](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=716-717&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a),
    a new payments::FullCardRequest is created, and
    [FullCardRequest::GetFullCard](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=47&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    is called
3.  Some checks occur, and then the [card unmask dialog is
    shown](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=78-79&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    while [Risk data is loaded in
    parallel](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=82-84&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
4.  [AutofillManager::ShowUnmaskPrompt](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=1056&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    calls
    [ChromeAutofillClient::ShowUnmaskPrompt](https://cs.chromium.org/chromium/src/chrome/browser/ui/autofill/chrome_autofill_client.cc?l=179&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a),
    which [creates a
    CardUnmaskPromptView](https://cs.chromium.org/chromium/src/chrome/browser/ui/autofill/create_card_unmask_prompt_view.h?l=20&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    and calls
    [CardUnmaskPromptControllerImpl::ShowPrompt](https://cs.chromium.org/chromium/src/components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.cc?l=45&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    with it
5.  On Desktop, a
    [CardUnmaskPromptViews](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/autofill/card_unmask_prompt_views.cc)
    is shown <center>**-- [ Card unmask dialog appears ] --**</center>
6.  On Desktop, if the dialog is accepted,
    [CardUnmaskPromptViews::Accept](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/autofill/card_unmask_prompt_views.cc?l=280&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    calls
    [CardUnmaskPromptControllerImpl::OnUnmaskResponse](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/autofill/card_unmask_prompt_views.cc?l=284&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    with the values entered on the dialog
7.  The CardUnmaskDelegate::UnmaskResponse is populated with these values, then
    the delegate’s OnUnmaskResponse [is
    called](https://cs.chromium.org/chromium/src/components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.cc?l=214&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
8.  [FullCardRequest::OnUnmaskResponse](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=92&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    uses these values to set up the PaymentsClient::UnmaskRequestDetails
9.  Once the user has accepted the dialog and Risk data is loaded,
    [FullCardRequest::SendUnmaskCardRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=134&rcl=354c7688915f6f86de4dd4b6b6d3cd5df7daafc2)
    calls
    [PaymentsClient::UnmaskCard](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=487&rcl=a4afafb52823c31b16c582ef1e59ee7bd57266e7)
10. [PaymentsClient::IssueRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=516&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    calls
    [PaymentsClient::InitializeUrlFetcher](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=530&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to [set the Payments
    URL](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=567&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to be called ([specified by
    UnmaskCardRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=205&rcl=90585e657db48f93bd73bc45d4caa975323da41b)),
    then [fires off an HTTP
    request](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=522-527&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to it <center>**-- [ GetRealPanAction called ] --**</center>
11. Google Payments receives the request and returns a response
12. [PaymentsClient::OnURLFetchComplete](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=589&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
    is called upon receiving a response, checks for errors, and [calls the
    request-specific RespondToDelegate
    function](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=654&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
13. [UnmaskCardRequest::RespondToDelegate](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=251&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    calls
    [AutofillManager::OnDidGetRealPan](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=1035&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a),
    which calls
    [FullCardRequest::OnDidGetRealPan](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=138&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
14. OnUnmaskVerificationResult [is
    called](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=144&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    on the `|ui_delegate_|`, which updates the UI based on the RPC’s result
    through a long chain of calls that ultimately end at
    [CardUnmaskPromptControllerImpl::OnVerificationResult](https://cs.chromium.org/chromium/src/components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.cc?l=76&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    and
    [CardUnmaskPromptViews::GotVerificationResult](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/autofill/card_unmask_prompt_views.cc?l=88&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
15. Back in FullCardRequest::OnDidGetRealPan, the RPC’s result [determines how
    to update the web page's
    form](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/full_card_request.cc?l=146&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
    *   On a failure,
        [AutofillManager::OnFullCardRequestFailed](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=1052&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
        clears the form
    *   On a success,
        [AutofillManager::OnFullCardRequestSucceeded](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=1041&rcl=849244ee60caf4ccc6a7defeddf0a221d4bdfb3a)
        logs the event and fills the form

## Chrome Upstream (upload credit card save)

1.  The user submits a form, triggering
    [AutofillManager::OnFormSubmitted](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=368&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d).
    If the form was autofillable, FormDataImporter::ExtractFormData [is
    called](https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_manager.cc?l=392-393&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
2.  An inner [FormDataImporter::ExtractFormData
    helper](https://cs.chromium.org/chromium/src/components/autofill/core/browser/form_data_importer.cc?l=169&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    is called, which begins the process of extracting both credit card and
    address profile information
3.  [FormDataImporter::ExtractCreditCard](https://cs.chromium.org/chromium/src/components/autofill/core/browser/form_data_importer.cc?l=307&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    is called which tries to detect if a new credit card was entered on the
    form, storing it in `imported_credit_card` if so
4.  [FormDataImporter::ExtractAddressProfiles](https://cs.chromium.org/chromium/src/components/autofill/core/browser/form_data_importer.cc?l=196&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    is called, which tries to [extract one address profile per form
    section](https://cs.chromium.org/chromium/src/components/autofill/core/browser/form_data_importer.cc?l=222&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    (maximum of 2)
5.  If the submitted form [included a credit
    card](https://cs.chromium.org/chromium/src/components/autofill/core/browser/form_data_importer.cc?l=120,123&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    and meets [Chrome Upstream
    requirements](https://docs.google.com/document/d/1Fz82dy8Puxgxwmm2lTADA4LQwDmnhFNJ2kshywlCdKQ),
    prepare to attempt to offer credit card upload by calling
    [CreditCardSaveManager::AttemptToOfferCardUploadSave](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=98&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
6.  The payments::PaymentsClient::UploadRequestDetails [is
    initialized](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=101-102&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d),
    which sets the card in the request <center>**-- [ Client-side credit card
    validation ] --**</center>
7.  The form is searched for a [valid CVC
    value](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=126-141&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d),
    setting it in the request if found, or taking note of the problem if not (no
    CVC field, empty CVC field, or invalid CVC value) <center>**-- [ Client-side
    address validation ] --**</center>
8.  [CreditCardSaveManager::SetProfilesForCreditCardUpload](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=314&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    is called, which performs Chrome-side Upstream profile verifications,
    logging any problems found in `|upload_decision_metrics_|` (no address,
    conflicting names, no name, conflicting postal codes, no postal code)
9.  If CVC is missing, the CVC fix flow is enabled, and no other problems were
    found, `|should_cvc_be_requested_|` [is set to
    true](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=156&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    in order to display the client-side CVC fix flow when upload is offered.
    Otherwise, if CVC is missing, `|upload_decision_metrics_|`
    [records](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=168&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    that it was not found
10. If any problems were found and the Send Detected Values experiment is
    disabled, offering upload save [is
    aborted](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=177-184&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
11. AttemptToOfferCardUploadSave [calls
    PaymentsClient::GetUploadDetails](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=199-203&rcl=ab8d0ea46daf7673a53524a3708f0ffd1ea9ee2d)
    with the set of seen addresses, detected values bitmask, and active Chrome
    experiments
12. [PaymentsClient::GetUploadDetails](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=495&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    prepares to issue a GetUploadDetailsRequest (a call to Google Payments’
    GetDetailsForSaveCard)
13. [PaymentsClient::IssueRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=516&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    calls
    [PaymentsClient::InitializeUrlFetcher](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=530&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to [set the Payments
    URL](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=567&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to be called ([specified by
    GetUploadDetailsRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=278&rcl=90585e657db48f93bd73bc45d4caa975323da41b)),
    then [fires off an HTTP
    request](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=522-527&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to it <center>**-- [ GetDetailsForSaveCardAction called ] --**</center>
14. Google Payments receives the request and returns a response
15. [PaymentsClient::OnURLFetchComplete](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=589&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
    is called upon receiving a response, checks for errors, and [calls the
    request-specific RespondToDelegate
    function](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=654&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
16. [GetUploadDetailsRequest::RespondToDelegate](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=331&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
    calls
    [CreditCardSaveManager::OnDidGetUploadDetails](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=232&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e),
    which decides how to proceed based on the GetUploadDetailsRequest’s
    response:
    *   [On a
        success](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=238&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e),
        the user is prompted to save the card to Google Payments, Risk data is
        loaded in parallel, and metrics note that upload was offered
    *   [On a
        failure](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=252&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e),
        if name+address+CVC were found, the user is prompted to save locally,
        and metrics note that upload was not offered
17. The final UMA and UKM metric decisions are [finally
    logged](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=310&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
18. ChromeAutofillClient::ConfirmSaveCreditCardToCloud offers to save the card
    to Google by surfacing an [infobar on
    Android](https://cs.chromium.org/chromium/src/chrome/browser/ui/autofill/chrome_autofill_client.cc?l=218-226&rcl=69f38cbb58152f8c9781a4d688adab1ad3c13cf6)
    or a [bubble on
    web](https://cs.chromium.org/chromium/src/chrome/browser/ui/autofill/chrome_autofill_client.cc?l=228-233&rcl=69f38cbb58152f8c9781a4d688adab1ad3c13cf6)
    <center>**-- [ Offer to save UI is shown ] --**</center>
19. Upon accepting save, the infobar/bubble fires a callback for
    [CreditCardSaveManager::OnUserDidAcceptUpload](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=491&rcl=69f38cbb58152f8c9781a4d688adab1ad3c13cf6)
    <center>**-- [ User clicked save ] --**</center>
20. Once both the user accepts save and Risk data is loaded,
    [CreditCardSaveManager::SendUploadCardRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=510&rcl=69f38cbb58152f8c9781a4d688adab1ad3c13cf6)
    is called, which [sets
    CVC](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=521-522&rcl=69f38cbb58152f8c9781a4d688adab1ad3c13cf6)
    in the request if the CVC fix flow was activated
21. [PaymentsClient::UploadCard](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=508&rcl=6ad45bcd758ad6eaba1da3a71b909f7ca7b46217)
    prepares to issue an UploadCardRequest (a call to Google Payments’ SaveCard)
22. [PaymentsClient::IssueRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=516&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    calls
    [PaymentsClient::InitializeUrlFetcher](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=530&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to [set the Payments
    URL](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=567&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to be called ([specified by
    UploadCardRequest](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=354&rcl=90585e657db48f93bd73bc45d4caa975323da41b)),
    then [fires off an HTTP
    request](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=522-527&rcl=90585e657db48f93bd73bc45d4caa975323da41b)
    to it <center>**-- [ SaveCardAction called ] --**</center>
23. Google Payments receives the request and returns a response
24. [PaymentsClient::OnURLFetchComplete](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=589&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
    is called upon receiving a response, checks for errors, and [calls the
    request-specific RespondToDelegate
    function](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=654&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
25. [UploadCardRequest::RespondToDelegate](https://cs.chromium.org/chromium/src/components/autofill/core/browser/payments/payments_client.cc?l=432&rcl=b7e2306fd4d8590a41f6fd103dfcc1013d6ca85e)
    calls
    [CreditCardSaveManager::OnDidUploadCard](https://cs.chromium.org/chromium/src/components/autofill/core/browser/credit_card_save_manager.cc?l=215&rcl=6ad45bcd758ad6eaba1da3a71b909f7ca7b46217),
    which, on a success, saves the credit card as a FULL_SERVER_CARD (so it
    doesn’t need to be unmasked on next use on the same device)

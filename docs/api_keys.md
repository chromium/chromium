# API Keys

When using a custom build, fork, or integration of Chromium, or if you're
building ChromiumOS, you will need access to Google API for key functionality.

[TOC]

*** note
**Note:** Software distribution with keys acquired for yourself is allowed, but
the keys themselves cannot be shared with parties outside the legal entity that
accepted the API ToS.  Keep in mind that a number of the APIs will have no or
very limited quota and not all of the APIs have additional quota available for
purchase.
***

**Googlers only:**

*   For a simpler approach to API keys, see https://go/chrome-api-key-setup
*   If you need a new API enabled in Chrome, use https://b/new?component=165132

**How-to:**
First, acquire API keys. Then, specify the API keys to use either when you build
Chromium, or at runtime using environment variables.

## Acquiring Keys

1.  Make sure you are a member of chromium-dev@chromium.org (you can choose
    not to receive mail).
    *** note
    **Note:** the APIs below are only visible to people subscribed to that group.
    ***
1.  Make sure you are logged in with the Google account associated with the email
    address that you used to subscribe to chromium-dev.
1.  Go to https://cloud.google.com/console
1.  _optional_  You may add other members of your organization or team on the Team
    tab.
1.  Open the **APIs and Services > Library** from the hamburger menu, search for
    all of the following APIs. For each of these APIs:
    1.  Click on it
    1.  Click `Enable API` button at the top
    1.  Read and agree to the Terms of Service
    1.  Check the `I have read and agree to the <API name> Terms of Service`
        checkbox
    1.  Click Accept.

    **List of APIs** (if any are not shown, recheck step 1 above):

    *   Cloud Search API
    *   Geolocation API (requires [enabling
        billing](https://developers.google.com/console/help/#EnableBilling) but
        is free to use; you can skip this one, in which case geolocation features
        of Chrome will not work)
    *   Google Drive API (enable this for Files.app on Chrome OS and
        SyncFileSystem API)
    *   Safe Browsing API
    *   Time Zone API
    *   Optional
        *   Admin SDK
        *   Cloud Translation API
        *   Geocoding API
        *   Google Assistant API
        *   Google Calendar API
        *   Nearby Messages API

1.  Go to the **Credentials** sub tab under the **API & Services** section in the
    hamburger menu.
1.  Click the `Create credentials` button then click on the **OAuth client
    ID** item in the drop-down list.
    *   Click on the `Configure consent screen` button. Fill in the "product
        name" (anything you choose) and other details, then click on `Save`.
    *   Return to the Credentials tab and click the `Add credentials` button
        again, then select "OAuth 2.0 client ID" from the drop-down list.
    *   In the "Application type" section check the **Other** option and give it a
        name in the "Name" text box, then click `Create`.
1.  In the pop-up window that appears, you'll see a **client ID** and a **Client
    secret** string. Copy and paste those in a text file on your dev box then
    click `OK` to dismiss. A new item should now appear in the **OAuth 2.0 client
    IDs** list. You can click on the name of your client ID to retrieve the ID and
    secret at any time. In the next few sections, these values will be referred
    to as the **Client ID** and **Client secret** fields.
1.  Click the `Create credentials` button _again_ on the same page.
    *   In the pop-over window that shows up, click the `API key` button.
    *   A pop-over should show up giving you the API key. Copy/paste in to a text
        file to save, although you will be able to access this as well.
    *   Click `OK` to dismiss.

You should now have an API key and an OAuth 2.0 client ID in the **Credentials**
tab. The next sections will refer to the value as the "API key".

*** note
**Note:** Your keys are not for distribution, and should not be shared with
others.
***

## Providing Keys at Build time

If you are building Chromium yourself, you can provide keys as part of your
build configuration, that way they are always baked into your binary.

Specify three variables in your `args.gn` file (edit by running `gn args
out/your_out_dir_here`)

```bash
google_api_key = "your_api_key"
google_default_client_id = "your_client_id"
google_default_client_secret = "your_client_secret"
```

## Providing Keys at Runtime

If you prefer, you can build a Chromium binary (or use a pre-built Chromium
binary) without API keys baked in, and instead provide them at runtime. To do
so, set the environment variables `GOOGLE_API_KEY`, `GOOGLE_DEFAULT_CLIENT_ID`
and `GOOGLE_DEFAULT_CLIENT_SECRET` to your "API key", "Client ID" and "Client
secret" values respectively.

On Chromium OS to specify the keys as environment variables append them to the
end of `/etc/chrome_dev.conf`:

```bash
GOOGLE_API_KEY=your_api_key
GOOGLE_DEFAULT_CLIENT_ID=your_client_id
GOOGLE_DEFAULT_CLIENT_SECRET=your_client_secret
```

## Signing in to Chromium is restricted

Signing in to Chromium requires an OAuth 2.0 token for authentication. As this
OAuth 2.0 token gives access to various Google services that handle user data
(e.g. Chrome sync), for security and privacy reasons the generation of this
OAuth 2.0 token is restricted. This means that signing in to Chromium is
restricted (as the OAuth 2.0 token cannot be generated). In order to sign in to
Chromium builds, please add your test account to
google-browser-signin-testaccounts@chromium.org (accounts in this group are
allowed to get access tokens bypassing the restriction above).

*** note
**Note:** Starting with Chromium M69, when the browser is set up with an OAuth
2.0 client ID and client secret, signing in with your Google Account to any
Google web property will also attempt to sign you in to Chromium (which will
fail as explained above). To avoid such errors, remove your OAuth 2.0 client
ID and client secret from your build to stop generating tokens when users sign
in to Google web properties (remove `google_default_client_id`,
`google_default_client_secret` from gn args and `GOOGLE_DEFAULT_CLIENT_ID`
and `GOOGLE_DEFAULT_CLIENT_SECRET` from your environment settings).
***

## Getting Keys for Your Chromium Derivative

Many of the Google APIs used by Chrome are specific to Google and not intended
for use in derived products. In the [API Console](http://developers.google.com/console)
you may be able to purchase additional quota for some of the APIs listed above.
**For APIs that do not have a "Pricing" link, additional quota is not available
for purchase.**

## Polyfilling chrome.identity API in Your Chromium Derivative

The default Chromium `chrome.identity.getAuthToken` API that extensions may
call to obtain auth tokens will fail outside of Google Chrome as the
implementation uses restricted APIs.

A prototype CL for Chromium embedders might use to replace the implementation
with one not dependent upon private APIs can be found attached to
[this post](https://groups.google.com/a/chromium.org/g/embedder-dev/c/tGCJ3QNVzYE).

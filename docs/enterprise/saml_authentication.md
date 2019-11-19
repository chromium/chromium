# SAML for Chrome OS login and enrollment

[TOC]

## SAML authentication in Chrome OS
By default, authentication for Google accounts is handled by Gaia, Google's
externally available account service. When an unauthenticated user visits a
[Google page](https://accounts.google.com), the GAIA login form is shown. This
form asks for the user’s e-mail address and GAIA password. When the user submits
this form, GAIA verifies that the credentials entered are correct and sets login
cookies.

Some enterprises use a more sophisticated model where authentication is handled
by a third-party IdP (identity provider). GAIA supports this model via the
industry-standard
[SAML protocol](https://en.wikipedia.org/wiki/Security_Assertion_Markup_Language).
A domain can be [configured](https://support.google.com/a?p=sso) to
use SAML authentication.

## Obtaining the user’s password
Chrome OS needs to know the user's password entered during login to
* encrypt the user’s data stored on the disk drive,
* protect the lock screen and to
* enable offline login when there is no network accessibility.

The problem with SAML is that the password is not entered on a Chrome OS system
dialog directly, but inside a webview hosted by the IdP. While the OS has access
to the HTML, there is no simple, canonical way to get the password as it is
unclear in general which form fields contain the data.

There are two ways to get user's password:

### Chrome Credentials Passing API
Google provides a
[Credentials Passing API](https://www.chromium.org/administrators/advanced-integration-for-saml-sso-on-chrome-devices)
that can be implemented by IdPs in Javascript on their SAML pages to pass the
required data to Chrome. GAIA uses this API, but any SAML IdP provider could use
it as well.

### Password scraping
Password scraping is used when the SAML IdP provider does not support the
Credentials Passing API.

In this method, the authentication screen injects a
[content script](https://cs.chromium.org/chromium/src/chrome/browser/resources/gaia_auth_host/saml_injected.js)
into the webview that hosts the login process. The content script looks for HTML
input fields of type password and copies their contents into an array. The array
is updated whenever the contents of a password field changes. Scraped passwords
are sent to a background page that accumulates them. This way, the password can
be captured even if the login flow spans multiple redirects to different HTML
pages.

At the end of the login flow, the array of scraped passwords is retrieved from
the background page.
Three cases are possible:
1. No password has been scraped. This will happen if the content script fails to
   locate the password in the HTML pages served by the IdP or if the IdP does
   not use traditional passwords at all.
2. Exactly one password was scraped. Most likely, this is the user’s password
   used for authentication.
3. More than one password was scraped. This may happen e.g. when the IdP
   requires the user to enter a permanent password and a one-time password into
   the login form.

Case 1 will cause Chrome OS login to ask the user to
[pick a manual password for the device](https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/login/screen_gaia_signin.js?rcl=c4dd0ee9aebc827a18caa7cb0fdcf7c123d1a29f&l=981).
If the password does not exist in the first place (e.g. authentication by smart
cards, NFC, biometry), Chrome OS login may
[proceed without the password](https://cs.chromium.org/chromium/src/chrome/browser/resources/gaia_auth_host/authenticator.js?rcl=faf24c60e6177fe0dcda857ec257d84ebabddc0e&l=799).

Case 2 is the ideal case. We most likely scraped the user’s password correctly.
Chrome OS login
[uses it as the user's password](https://cs.chromium.org/chromium/src/chrome/browser/resources/gaia_auth_host/authenticator.js?rcl=faf24c60e6177fe0dcda857ec257d84ebabddc0e&l=708).

Case 3 indicates that we probably scraped the user’s actual password and some
additional password fields containing additional credentials that are not of
interest to Chrome OS. In order to determine which one is the correct password,
Chrome OS asks the user to enter the password once more into an additional
password prompt. If the password entered matches one of the scraped password,
the user’s actual password has been identified and login is successful. The user
can try again if there is no match. After two mismatches, login fails with an
error message.

## Enterprise enrollment
For [Enterprise enrollment](enrollment.md) the enrolling user’s email address is
needed to associate the device with the correct domain. The email is sent from
the Device Management (DM) Server to Chrome in the
[username field of the PolicyData message](https://cs.chromium.org/chromium/src/components/policy/proto/device_management_backend.proto?rcl=d477c3a9479cbebc4c7c36b7b89d641abda404a2&l=448)
during device
[policy fetch](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/policy/enrollment_handler_chromeos.cc?l=316&rcl=176048587047ce83551542b20d973b2ff698cc5f).
The domain name is extracted
[here](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/policy/enrollment_handler_chromeos.cc?rcl=d477c3a9479cbebc4c7c36b7b89d641abda404a2&l=576).

There is no need to determine the user’s password.

## Instructions for Google Employees
See [go/cros-prd-saml](https://goto.google.com/cros-prd-saml) for PRD and test
credentials.

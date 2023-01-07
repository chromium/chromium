# First user run

When a user first signs in to a Chrome OS device, they are run through first-run
OOBE flow. From UX perspective, the flow is considered part of the first user
sign in, even though it happens after the user cryptohome is created and the
user session has started. The session will not be considered active until the
user first-run flow finishes.

The user first run flow contains the following steps:

## Terms of Service screen

The first and only screen shown during public session login. It's shown if device
local account policy specifies terms of service URL. The screen will display the
plain text content downloaded from the provided URL (non plain-text content will
be rejected). The public session will start only if the user accepts the
provided terms of service. If the terms are rejected, public session will exit
immediately.

Terms of service screen is only shown for public session accounts.

## Sync Consent screen

Shown on official Chrome builds only, and only for users with GAIA account.
It informs the user about Chrome sync, and Google services personalization.
The user has an option to request to review of sync options after user setup
finishes.
The sync consent screen will not be shown if sync is disabled by policy.

## Fingerprint Setup screen

Screen shown if the device supports fingerprint unlock. The user can use the
screen to enroll their fingerprints for unlocking their user session.
For more information see [fingerprint documentation](fingerprint.md).

## Discover screen

The screen is shown if the device is in tablet mode, and if PIN authentication
is enabled. The screen can be used to set up PIN that can be used for unlocking
the user session.

## ARC Terms of Service screen

The screen shows Google Play Terms of Service, if Android apps are supported on
the device. The Terms of Service content is fetched from play.google.com, and
hosted in a webview within the ARC Terms of Service screen. To reduce time
required to display the webview content, the content fetch is initiated when
GAIA screen is first shown, and later refreshed as required (for example if the
locale changes).

If the screen is skipped, user session will continue with Android apps support
disabled. The screen cannot be skipped on Chrome OS devices that use Andriod P
and later.

To effectively test this screen in browser tests, the test has to set up an
embedded test server instance to serve terms of service content, and override
the URL from which terms of service are fetched using a test API exposed by the
screen implementation.

## Recommend Apps screen

Screen shown when ARC terms of service are accepted by the user. It lists
Android apps the user has installed on their other devices and that would work
on the host Chrome OS device. The user can select the set of apps they want to
install. Once the selection is confirmed, Chrome will start installing the
selected apps. The user will be informed that the app installation has started,
and the number of installing apps using App Downloading screen.

The list of recommended apps is fetched from  Google Play, and is calculated
using the Chrome OS device settings sent with the recommended apps list request.
The settings for example include supported ARC features, display, and
touchscreen characteristics.

# Assistant Opt-in Flow screen

Screen that sets up Google Assistant on Chrome OS, if assistant is available on the
device. Assistant can be enabled using `--enable-native-google-assistant`
switch.

The assistant opt-in screen loads value proposal content in webview - browser tests
should override the value proposal URL to a URL served by an embedded test server
set up by the test (otherwise assistant screen will not get past loading UI).
The value proposal UI will ask the user for consent to enable assistant, if
required.

Value prop UI is followed by third-party disclosure screen. Both value prop and
third-party disclosure may be skipped if assistant service deems they are not
required for the user.

If voice match is supported on the device, assistant opt-in flow offers the user
to enroll to use their voice as an ID. The user can skip the speaker ID
enrollment, if they wish.

The final step of the opt-in flow is informing the user the Assistant is ready,
and offering to enable some advanced features, if any are available.

Much of this flow is managed by ash::assistant::AssistantSettings service.
To effectively test the screen flow, this service should be faked by the test.

# Multi-device Setup screen

Screen to set up the Chrome OS device to work with other user's devices, for
example setting up their Android phone to be used for Smart Lock. The screen
will be shown only if the user has available remote devices.


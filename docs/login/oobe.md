# Out Of Box Experience - OOBE

Out Of Box Experience, or *OOBE*, is a flow that goes through several sequential
steps to set up new, unowned device. A device is owned when it's
*   enterprise enrolled, or
*   at least one user has been added to the device.

In the former case, the device is owned by the enrollment domain, and device
settings are controlled by the device policy specified by the domain
administrator.

If the device is not enterprise enrolled, the first user to be added to the
device becomes the device owner. The owner user cannot be removed unless the
device is power-washed.

During device OOBE setup the user goes through the following steps:

##  Welcome screen

The initial OOBE step that welcomes the user to their new device, and provides
options to change settings with impact on general usability:
*   language and keyboard layout
*   accessibility settings

Note that Chromevox, a screen reader bundled with Chrome, can be activated at
any time by pressing `Ctrl + Alt + Z` key combination.

On devices in system dev-mode, the Welcome screen UI shows a link to enable
Chrome OS system debugging features. Enabling the debugging features will
install debugging tools on the Chrome OS device that are commonly present on
Chrome OS test images only.

##  Network screen

Screen asking the user to select and connect to a network. The screen will block
the rest of the OOBE setup until a network is connected because the screens that
follow may require network connectivity. The exception is offline Demo Mode
setup, which is expected to work fully offline - for more information on demo
mode see [demo mode](demo_mode.md).

## EULA screen

The screen that shows the End-User Licence Agreement (EULA) to the user, and
waits until the user accepts them. The setup will not continue until the user
accepts the terms. The screen has an option to return back to the network
setup screen. Accepting EULA terms will also initialize:
*   metrics and crash collection, provided that the user agreed to it
*   network portal detection
*   timezone resolver
*   OEM OOBE customization

This screen is shown on official Chrome builds only.

##  Update check screen

Update check screen will perform online Chrome OS update availability check
(using system update engine dbus service). If a critical update to a newer
Chrome OS version exists, the update screen will wait for the update engine to
download and install the update, and then request Chrome OS reboot. The screen
will give the user feedback about the update progress.

An update is considered critical if Omaha, the service that serves Chrome OS
updates, specifies a deadline for applying the update.

Non-critical updates are ignored during OOBE - the update engine will download
them and install in the background after OOBE finishes.

If the update engine detects that the device is currently using a metered
connection (e.g. cellular network), the update screen will ask for user
confirmation before proceeding with the update. If the user declines, OOBE will
go back to the network selection screen.

Note that update screen requires network connectivity - it will check whether
the current network is online, and show a network error screen if that is not
the case. The network error screen is similar to network screen shown earlier in
OOBE in the sense that it provides UI to select and connect to a network, but
*   is not part of OOBE flow itself - when error screen is hidden, the
    flow will generally return to the screen shown prior to the error screen
*   provides more options for fixing connectivity issues - for example, options
    to start guest user login, captive portal authentication etc.

## Re-enrollment (Auto Enrollment) check

A transient screen shown while Chrome checks the devices re-enrollment state,
and whether the device has been disabled remotely. Depending on the result of
the checks OOBE might:
*   show enrollment screen, if the re-enrollment is recommended or forced - in the
    former case the user will not be able to dismiss the enrollment screen
*   show a screen informing that the device is disabled and block further setup
*   show user GAIA login prompt

Re-enrollment check screen depends on network connectivity, and will display
network error screen if the device is not online.

For more information on enrollment scenarios, including re-enrollment see
[enrollment documentation](../enterprise/enrollment.md).

If re-enrollment was not requested, OOBE will be marked as completed at this
point. Otherwise, OOBE will be marked as completed once enrollment screen shown
after auto-enrollment screen reports enrollment is done. Note that enrollment
screen finishing does not imply the device is actually enrolled, unless the
re-enrollment is forced.

## GAIA sign-in screen

The first screen shown after OOBE completion. The screen shows GAIA
authentication UI hosted inside a Chrome webview. User authentication is
completely handled by GAIA. Once the user is authenticated with GAIA, the
screen reports authenticated credentials to Chrome. The credentials are used to
create user cryptohome, and add the user to the device. Additionally,
data required to authenticate the user with Google services is read from
the cookies in the webview used for GAIA authentication. This data (auth code)
will be used to generate a refresh token, which will be stored locally, and used
to generate user's access tokens as needed. This allows the user to use Google
services without having to manually reauthenticate.

In addition to initial user login, GAIA screen can be shown for users that
already exist on the device:
*   if user fails to enter correct password needed to decrypt their cryptohome
    multiple times
*   if a change to the user's GAIA credentials is detected, the user will be
    required to go through online GAIA sign-in, so local credentials can be
    updated accordingly. Note that the user will not be able to access local
    data using the updated credentials - in order to retain local data, they
    will be asked to enter their old credentials.

On enterprise enrolled devices, GAIA screen supports user authentication using
SAML and Active Directory flows - for more information, see documentation in
docs/enterprise.

## Enrollment screen

Screen that hosts enrollment UI inside a webview - for more information on
enrollment flows, see [enrollment documentation](../enterprise/enrollment.md)

## Useful shortcuts

`Ctrl + Alt + Z` - toggle Chromevox, a screen reader bundled with Chrome.

`Ctrl + Alt + E` - start enrollment flow, if the device is still unowned.

`Ctrl + Alt + D` - start Demo mode setup - supported on Welcome screen only

`Ctrl + Alt + R` - initialize powerwash - this can be used throughout OOBE,
    and on the login screen, but might be disabled on enterprise enrolled
    devices.

`Ctrl + Alt + Shift + X` - enable debugging features - available during OOBE
    only.

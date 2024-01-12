# Chrome OS Demo Mode

Demo Mode is Chrome OS device set-up intended to run in retail environment. The
goal is to let retail customers try out Chrome OS devices, and get a feel of
their capabilities, including running Android apps.

Demo Mode is implemented as a Managed Session, by enrolling the device into a
special demo domain, using "cros-demo-mode" requisition. Enrollment will set up
policies that control Demo Mode sessions, and specify auto-launched Demo Mode
device local account. Additionally, Demo Mode setup will install cros-component
containing assets needed to run Demo Mode. For example:
*   demo Android apps
*   sample photos
Using assets from the installed cros-component enables Demo Mode to run offline
after it's been set up.

## Setup

Demo Mode can be set up on unowned devices only. Demo Mode setup is initiated
from the Welcome Screen in OOBE by pressing `Ctrl + Alt + D` key combination, or
tapping the screen 10 times. This will show a prompt asking the user to confirm
they want to set up Demo Mode. Accepting the prompt will start off Demo Mode
setup.

Demo Mode setup goes through the following steps:

1.  Demo Preferences screen - configures the language, country, and keyboard
    layout to be used in demo sessions.
1.  Network Selection screen - selects network to be used during the rest of
    demo mode setup.
1.  EULA screen, if EULA was not previously accepted.
1.  ARC Terms of Service - non skippable ARC Terms of Service adjusted for demo
    mode.
1.  Update Check screen.
1.  Auto Enrollment Check, and Device Disabled Check, that check whether the
    device should be disabled, or re-enrolled.
1.  Demo Mode Setup screen - screen that sets up Demo Mode. This step installs
    Demo Mode Resources cros-component, and enrolls the device into the Demo
    Mode domain.

Demo Mode setup can be cancelled up to the last step.

## Offline-Enrolled Demo Mode

Provided that they have Offline Demo Mode Resources cros-component installed on
the device's stateful partition, demo mode can be set up using offline Demo Mode
set up. Offline Demo Mode setup does not require communication with the DM
Server, thus requiring no network connectivity. The device will be enrolled in a
fake domain, using a signed Demo Mode policy blob loaded from the offline-demo-mode
enabled device. Offline Demo Mode setup can be started by skipping network
selection in the Network screen (an option only available on devices that have
Offline Demo Mode Resources pre-installed).

Offline Demo Mode Resources cros-component is a pre-installed version of Demo
Mode Resources cros-component that includes resources that would normally be
downloaded during online Demo Mode setup - for example, the Highlights App (an
app that highlights Chrome OS features).

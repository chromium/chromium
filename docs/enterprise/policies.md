# Enterprise policies

Under enterprise management, organization admins can configure the way a
ChromeOS device / browser operates using policies.

On most operating systems, policies are applied to specific users / all users
of the browser, but on ChromeOS there are also policies that control the device
itself.

On all platforms, cloud-based policies are fetched and applied when a managed
user signs in.

[TOC]

## Policy sources

On different operating systems, there can be different methods for an enterprise
to propagate policies for all users (including non-managed ones):

**Windows** Policies can be set up via Windows Registry ([GPO](https://en.wikipedia.org/wiki/Group_Policy)).

**MacOS** Policies can be set via Mac preferences system.

**Linux** Policies can be set via files in specific directories:

The base directory is `/etc/chromium/policies` for Chromium builds,
 `/etc/opt/chrome/policies/` for official Chrome builds.
The base directory contains two subdirectories: `managed/` for mandatory
policies and `recommended/` for recommended policies. All files inside these
directories are treated as JSON files containing policies.

On these systems it is also possible to set machine-wide cloud-based policies.

** Chrome OS **

Chrome OS devices can be either cloud-managed or Active Directory managed
([AdManagement](https://support.google.com/chrome/a?p=ad)).

The cloud source is usually called DMServer (device management server).
Organization admins can configure both device and cloud policies using
https://admin.google.com.

On an Active Directory managed device policies are set via GPO from the Active
Directory server.

When Chrome OS is in development mode with writeable rootfs, it would also
apply use `Linux` policy mechanism, that can be handy for testing during
development.

## Policy types (ChromeOS)

There are two main types of policies on Chrome OS: User policies, that define
the browser behavior for a particular user, and Device policies, that control
the whole device.

## Device policies

Device policies are defined in the [chrome_device_policy proto file](https://cs.chromium.org/chromium/src/components/policy/proto/chrome_device_policy.proto).
They are also mentioned in the [policy templates file](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json)
with `'device_only': True` for documentation purposes.

Device policies are applied to all users on the device (and even if no user
is signed in), even for guest sessions. Note that device policy can also limit
which users can sign in on the device.

Implementation-wise, these policies can have complex structure, and are
usually accessed via
[DeviceSettingsProvider](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/settings/device_settings_provider.h)
or its wrapper [CrosSettings](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/settings/cros_settings.h).

## User policies

User policies are defined in the [policy templates file](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json);
only entries without `'device_only': True` are user policies.

User policies are bound to user accounts, so a personal account on
an enterprise-enrolled device would be affected only by device policy, while
an enterprise-owned account on a personal device would only be affected by user
policy for that account.

### ChromeOS
Chrome OS has some specific enterprise-only account types ([Kiosk, public
accounts](kiosk_public_session.md)) that are controlled via policies. Those
policies are usual user policies, though they have their own user ID namespace
when retrieved from DMServer.

### Windows/MacOS/Linux
Chrome on these systems can be configured to receive machine-wide cloud-based
policy from DMServer. It is a user policy, but it would be applied to all
users.

## Extension policies

Organization admins can [configure particular extensions](https://www.chromium.org/administrators/configuring-policy-for-extensions)
for the user. Such extensions have to define the schema of the configuration
in their manifest.

When a Chrome OS device is cloud-managed, there is a limit on policy size.
As such configuration can be relatively large, it is not provided as a part
of user policy. Instead, user policy will only include URL and hash signature
for external data, and browser will fetch that data, validate its signature,
validate the data against the schema from extension manifest, and provide the
extension with such configuration.

The same approach is used for other large objects that can be set via
policies (e.g. background wallpapers or printer configuration).

## Adding new policies

See [adding new policies HowTo](https://www.chromium.org/developers/how-tos/enterprise/adding-new-policies).

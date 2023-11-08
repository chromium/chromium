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

The base directory is:

- `/etc/chromium/policies` for Chromium builds,
- `/etc/opt/chrome/policies/` for official Chrome builds,
- `/etc/opt/chrome_for_testing/policies/` for official Chrome for Testing builds.

The base directory contains two subdirectories: `managed/` for mandatory
policies and `recommended/` for recommended policies. All files inside these
directories are treated as JSON files containing policies.

On these systems it is also possible to set machine-wide cloud-based policies.

**ChromeOS**

ChromeOS devices can be either cloud-managed or Active Directory managed
([AdManagement](https://support.google.com/chrome/a?p=ad)).

The cloud source is usually called DMServer (device management server).
Organization admins can configure both device and cloud policies using
https://admin.google.com.

On an Active Directory managed device policies are set via GPO from the Active
Directory server.

When ChromeOS is in development mode with writeable rootfs, it would also
apply use `Linux` policy mechanism, that can be handy for testing during
development.

## Policy types (ChromeOS)

There are two main types of policies on ChromeOS: User policies, that define
the browser behavior for a particular user, and Device policies, that control
the whole device.

## Device policies

Device policies are defined in the [chrome_device_policy proto file](https://cs.chromium.org/chromium/src/components/policy/proto/chrome_device_policy.proto).
They are also mentioned in the [policy templates files](https://cs.chromium.org/chromium/src/components/policy/resources/templates/)
with `'device_only': True` for documentation purposes.

Device policies are applied to all users on the device (and even if no user
is signed in), even for guest sessions. Note that device policy can also limit
which users can sign in on the device.

Implementation-wise, these policies can have complex structure, and are
usually accessed via
[DeviceSettingsProvider](https://cs.chromium.org/chromium/src/chrome/browser/ash/settings/device_settings_provider.h)
or its wrapper [CrosSettings](https://cs.chromium.org/chromium/src/chrome/browser/ash/settings/cros_settings.h).

## User policies

User policies are defined in the [policy templates files](https://cs.chromium.org/chromium/src/components/policy/resources/templates/);
only entries without `'device_only': True` are user policies.

User policies are bound to user accounts, so a personal account on
an enterprise-enrolled device would be affected only by device policy, while
an enterprise-owned account on a personal device would only be affected by user
policy for that account.

### ChromeOS
ChromeOS has some specific enterprise-only account types ([Kiosk, public
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

When a ChromeOS device is cloud-managed, there is a limit on policy size.
As such configuration can be relatively large, it is not provided as a part
of user policy. Instead, user policy will only include URL and hash signature
for external data, and browser will fetch that data, validate its signature,
validate the data against the schema from extension manifest, and provide the
extension with such configuration.

The same approach is used for other large objects that can be set via
policies (e.g. background wallpapers or printer configuration).

## Adding new policies

See the [adding new policies guide](add_new_policy.md#adding-a-new-policy).

## Policy Ownership

Policy owners can be individuals, references to OWNERS files, or Google Group
aliases.

Each policy has two or more owners to minimize the risk of becoming orphaned
when the author moves away from it.
At least one of the owners listed for a policy needs to be an individual,
preferably one with a chromium.org or google.com account. This is to ensure
that some external organizations like the translators team can more easily
reach out in case of questions.
At least one of the owners should be a reference to an OWNERS file or a Google
Group. OWNERS files are preferred to groups since groups can have restricted
access rights.

### Responsibilities of the Policy Ownership

The policy owner is expected to be familiar with the field which the policy
affects so that they can help triage and fix issues with the functioning of the
policy or answer questions about it. In many cases the policy owner might not be
the author of the policy but they are still expected to fulfill the aforementioned
obligations to the best extent possible. Potentially by enlisting help from other
team members when necessary.

### Orphaned Policies

There are many policies where the ownership chain has been interrupted irreversably.
In these cases the enterprise team members are enlisted as owners of such policies
on a random principle. The owner in this case is not expected to always be able to
address issues on their own. They should however assume ownership of the issue and
seek out resolution by triaging severity and organizing the required resources to
resove it.

If the randomly assigned ownership is not suitable it is still a responsibility of
the assigned owner to find a better owner and drive the transfer of ownership.

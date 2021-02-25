# Manage Chrome OS devices with Active Directory®

Enterprise customers may integrate their Chrome OS devices into a Microsoft®
Active Directory® (AD) environment. This integration joins devices to the AD
domain. Users do not need Gaia identities; they sign in using their AD
credentials. Admins manage sessions and push policies to users and devices from
their AD servers using group policy. There is no need to synchronize users to
Google.

[Google Chrome Enterprise Help article](https://support.google.com/chrome/a?p=ad)

[Troubleshoot Active Directory](https://support.google.com/chrome/a?p=troubleshoot_ad)

[TOC]

## Integration with enterprise enrollment flow
Google domains can either be set up for (regular) cloud management or AD
management. If during [enterprise enrollment](enrollment.md) a device is
registered with a domain set up for AD management, the device management (DM)
server replies with
[DeviceRegisterResponse::CHROME_AD](https://cs.chromium.org/chromium/src/components/policy/core/common/cloud/cloud_policy_client.cc?l=45&rcl=506aea9166170a6ecb7ab5ecbf30b21626d5e14b),
which turns the device into
[DEVICE_MODE_ENTERPRISE_AD](https://cs.chromium.org/chromium/src/components/policy/core/common/cloud/cloud_policy_constants.h?rcl=a2aecfd5286d50ba833241f351f32e512ceb3351&l=142).
This mode gets written to install attributes. For devices in this mode we show
an additional
[step](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/login/enrollment/enrollment_screen.cc?rcl=a2aecfd5286d50ba833241f351f32e512ceb3351&l=535)
for Active Directory® domain join.

## Active Directory® sign-in
If a device was joined to an AD domain, Chrome OS shows a custom
[dialog](https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/login/screen_gaia_signin.html?rcl=298d950825cb079fbd6b05c3b84b2806c287afa5&l=23)
for user sign-in.

## Communication with AD server
Chrome does not talk to the AD server directly. Instead, all communication, i.e.
domain join, user auth, policy fetch, user status queries, Kerberos files queries,
is relayed through the
[authpolicy](https://cs.corp.google.com/chromeos_public/src/platform2/authpolicy/README.md)
system daemon.

## Policies
Policies pulled from AD group policy objects (GPOs) have
[POLICY_SOURCE_ACTIVE_DIRECTORY](https://cs.chromium.org/chromium/src/components/policy/core/common/policy_types.h?l=43&rcl=fdd7489f1f95a984371c03e118ff17a460c659f8),
which translates to "Local Server" on the [Chrome policy page](chrome://policy).
The conversion from GPO to protobuf happens in
[DevicePolicyEncoder](http://cs/chromeos_public/src/platform2/authpolicy/policy/device_policy_encoder.h?l=30&rcl=34f2f96b8af4677048f3f3d21e24d507618187ef)
and
[UserPolicyEncoder](http://cs/chromeos_public/src/platform2/authpolicy/policy/user_policy_encoder.cc?l=30&rcl=34f2f96b8af4677048f3f3d21e24d507618187ef).
Note that a
[protofiles uprev](http://cs/chromeos_public/src/third_party/chromiumos-overlay/chromeos-base/protofiles/protofiles-0.0.32.ebuild?l=26&rcl=735ecdbf0d4101a07558147d1e6ab4d7c45ad7aa)
is necessary to get the latest policies.

## Chrome Architecture
The following Chrome classes are most relevant for the AD integration:
[AuthPolicyClient](https://cs.chromium.org/chromium/src/chromeos/dbus/authpolicy/authpolicy_client.h)
is the D-Bus client for the authpolicy daemon. All authpolicy D-Bus calls are
routed through it. The
[AuthPolicyHelper](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/authpolicy/authpolicy_helper.h)
is a thin abstraction layer on top of the
[AuthPolicyClient](https://cs.chromium.org/chromium/src/chromeos/dbus/authpolicy/authpolicy_client.h)
to handle cancellation and other stuff. The
[AuthPolicyCredentialsManager](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/authpolicy/authpolicy_credentials_manager.h)
keeps track of user credential status, shows notifications if the Kerberos
ticket expires and handles network connection changes. The
[ActiveDirectoryPolicyManager](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/policy/active_directory_policy_manager.h)
is the AD equivalent of the CloudPolicyManager and handles policy for AD-managed
devices.

## Google services
Users do not need a Google identity to sign in and Chrome is not signed in.
Thus, no Google services are available by default unless the user signs in from
the content area.

Moreover, users may sign up for a Play Store account from within their user
session, see step 5 of the
[Help article](https://support.google.com/chrome/a?p=ad).
For this purpose, DM Server creates a LaForge account for the user. A LaForge
account is a shadow Gaia account with scope limited to the Play Store. To prove
the user's identity, a SAML flow is employed with DM Server as service provider
and AD (or really any other) as identity provider. The SAML flow is triggered by
[ArcActiveDirectoryEnrollmentTokenFetcher](https://cs.chromium.org/chromium/src/chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h).

### Instructions for Google Employees
See [go/cros-ad-test-env](https://goto.google.com/cros-ad-test-env) for setting
up an Active Directory® test environment.

See [go/streamlinesteps](https://goto.google.com/streamlinesteps) to check out
streamline domain join.

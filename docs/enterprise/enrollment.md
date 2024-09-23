# Enterprise Enrollment on ChromeOS

Enterprise Enrollment is a process that marks a device as belonging to
particular organization and enables [management](management.md) of the device
by organization admins.

[TOC]

## Requirements

Only **devices without owner** can be enrolled. Ownership of the device is
established either during Enterprise Enrollment (the organization becomes the
owner of the device) or during first user sign-in (in this case this user
becomes the owner of the device).

Ownership of the device can be reset using factory reset (```Ctrl+Alt+Shift+R```
on the login screen), if it is not disabled via device policy.

Developers can reset ownership by running following commands as root in shell:
```
pkill -9 chrome
rm -rf /home/chronos/Local\ State /var/lib/whitelist /var/lib/devicesettings /var/lib/device_management /home/.shadow
rm /home/chronos/.oobe_completed
crossystem clear_tpm_owner_request=1
reboot
```

Only **enterprise users** can enroll devices (device will be owned by the
organization user belongs to).

#### Instructions for Google Employees
Are you a Google employee? See
[http://go/managed-devices/faq/using-yaps](https://goto.google.com/managed-devices/faq/using-yaps)
to learn how to use simple development device management server.

See
[http://go/managed-devices/faq/test-account](https://goto.google.com/managed-devices/faq/test-account)
for instuctions on how to get enterprise account for testing.

## Enrollment scenarios

There are several enrollment scenarios, exact choice is made based on
following factors:
 * How the authentication is performed
 * If enrollment can be avoided by user
 * What initiates enrollment.

#### Instructions for Google Employees
Are you a Google employee? See
[go/chromeos-enrollment-overview](https://goto.google.com/chromeos-enrollment-overview)
for other enrollment scenarios in development.

### Manual enrollment

Enrollment can be triggered manually on the login screen via `Ctrl+Alt+E`
shortcut. User will have to authenticate using username/password. User can
cancel enrollment attempt and return to login screen.

### Re-enrollment

During initial setup device queries management service to check if it was
previously enrolled, and if organization admins indicated that device should
be enrolled again.

This is set on https://admin.google.com/ under `Enrollment & Access` section on
`Device Management>Chrome>Device Settings` page.

Authentication is the same as in **Manual enrollment** case, and whether
enrollment can be skipped depends on policy set by admins.

### OEM-triggered Enrollment

Device manufacturers can provide special [OEM manifest](https://cs.chromium.org/chromium/src/chromeos/ash/components/system/statistics_provider.cc?rcl=2e366a611abdd2be6995e625f3281d40fab5b5e3&l=83)
that controls if device should be enrolled, and if enrollment is forced.
Authentication is the same as in **Manual enrollment** case.

### Offline demo-mode enrollment

This mode is intended for demo ChromeOS features e.g. in retail stores. This
enrollment does not require network connection, it enrolls device to a fixed
domain and uses policy from a local resource.

Demo enrollment can be triggered during initial setup on welcome/network
screens via `Ctrl+Alt+D` shortcut. No authentication is required during
enrollment.

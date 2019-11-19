# Fingerprint Enrollment

Fingerprint scanners are an easy way for users to authenticate to their devices.
As per current policy, up to 3 fingerprints can be enrolled per user per device.
On devices that have fingerprint sensors, users can enroll their fingerprints:
1.  during the user login OOBE in the Fingerprint Enrollment screen
2.  in lock screen settings page (chrome://settings/lockScreen).

The OOBE Fingerprint Enrollment screen is shown in the middle of OOBE flow and
can be skipped. It prompts the user to enroll a fingerprint up to 3 times and
shows progress during enrollment.

The lock screen settings page is hidden behind a password prompt that creates
an authentication token. This auth token is needed to successfully enroll
fingerprints. Enrollment can only be started if the user has less than 3
fingerprints enrolled.

While fingerprint setup depends on biod system dbus service, Chromium implements
a fake dbus client (`FakeBiodClient`) that fakes interactions with the biod
dbus service. Tests can use `FakeBiodClient` to create an enrollment session and
simulate fingerprint enrollment scan progress.

Fingerprint unlock policy is dependent on device policy and hardware specs.
Fingerprint can be manually enabled for testing by calling:
chromeos::quick_unlock::EnabledForTesting(true) to force enable fingerprint features.

Fingerprint unlock is not considered strong authentication. This means that,
under certain policies, fingerprint cannot be the only method of authentication
used over an extended period of time. After a designated amount of time, the
user's is notified that a password must be used to login which will mark strong
auth and re-enable fingerprint unlock.

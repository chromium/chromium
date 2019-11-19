# Kiosk mode and public sessions (Chrome OS)

When Chrome OS device is enterprise enrolled, organization admins can add two
special types of users on the device. Those are Public sessions and Kiosk apps.

## Public sessions

Public session can be described as managed guest session: guest mode that is
controlled by user policy.

It does not have any real google account, it is ephemeral (no data would be
persisted on the device after session is ended), but organization admins
still have some control over that: they can pre-install extensions, set
policies, install certificates.

To set up public session, go to [Admin console](https://admin.google.com) under
```Device Management > Chrome Management > Device Settings```, ```Kiosk
settings``` section, and set ```Public session kiosk``` setting to
```Allow```. You can also configure additional settings.

## Kiosk mode

Kiosk mode is a session that runs a single Chrome/Android app.

It does not have any real google account, it is persistent (data will be
persisted between kiosk sessions) by default.

Multiple kiosk apps are allowed per device, and they can be launched from system
shelf on the login screen. Additionally, the administrator can set up one app to
launch automatically on start-up, in which case login screen is not shown unless
the user cancels the app launch from the app launch splash screen.

To set up kiosk mode, go to [Admin console](https://admin.google.com) under
```Device Management > Chrome Management > Device Settings```, ```Kiosk
settings``` section, add available apps under ```Kiosk apps``` / ```Manage
Kiosk Applications```.

You can configure kiosk app settings under ```Device Management > Chrome
Management > Apps & Extensions```

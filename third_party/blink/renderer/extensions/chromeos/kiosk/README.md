# Chrome OS Kiosk Mode Extensions

This directory defines the `chromeos.kiosk` collection of APIs restricted to
Chrome OS devices in Kiosk mode.

Kiosk mode APIs usually fall in one of the categories:

*   Specific APIs that are mostly relevant for Kiosk web applications, such that
    we don't want to ship or enable it in all sessions.
*   High "privilege" APIs that would pose a security or privacy risk if enabled
    in all sessions.

Consider implementing new APIs in the top level `chromeos` or other namespace if
the above points do not apply. You can then employ other mechanisms, like prefs
and policies, to make the API available for the Kiosk application.

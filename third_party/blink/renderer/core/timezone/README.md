# core/timezone

This directory contains code which manages the time zone associated with
a renderer. At this point the functionality involves setting ICU's default
time zone and notifying V8 and workers.

Most of the time, TimeZoneController just listens to mojo notifications sent
by TimeZoneMonitor service that watches host system time zone changes and sets
the time zone accordingly.

Time zone override mode allows clients to temporarily override host system
time zone with the one specified by the client. When the override is removed,
the current host system time zone is assumed.

Time zone override functionality is exposed through the DevTools protocol method
Emulation.setTimezoneOverride.

The time zone identifier format is compatible with **IANA time zone database**,
also known as **Olson database**. The list of available time zone identifiers
can be found in the [List of tz database time zones][1]

## See Also
[1]: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones

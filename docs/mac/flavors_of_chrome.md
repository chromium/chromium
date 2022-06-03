# Flavors of Google Chrome for the Mac

There are various flavors of Google Chrome that are available. From the user’s
perspective, there are four channels: The
[stable channel](https://www.google.com/chrome/), the
[beta channel](https://www.google.com/chrome/beta/), the
[dev channel](https://www.google.com/chrome/dev/), and the
[canary channel](https://www.google.com/chrome/canary/).

However, there are two different flavors of the Beta and Dev channels. The older
flavors of the Beta and Dev channels share the same bundle ID and the same user
data directory location as the Stable channel, and therefore cannot be run at
the same time as the Stable channel. These flavors are called “non side-by-side”
or “non-SxS”, as they cannot be run side-by-side to the Stable channel of Google
Chrome.

Newer flavors of the Beta and Dev channels have different bundle IDs and
different data directory locations from the Stable channel, and can be run at
the same time as the Stable channel. These flavors are called “side-by-side” or
“SxS”.

Chrome Canary has always been side-by-side capable.

The easiest way to distinguish a side-by-side Beta or Dev Chrome from a non
side-by-side one is that the side-by-side flavor has a different icon that
specifies “Beta” or “Dev”. Other differences are specified in the table below.

| Flavor                       | Bundle ID                  | Creator Code | User Data Directory location                          |
|------------------------------|----------------------------|--------------|-------------------------------------------------------|
| Google Chrome Stable         | `com.google.Chrome`        | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        |
| Google Chrome Beta (non-SxS) | `com.google.Chrome`        | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        |
| Google Chrome Dev (non-SxS)  | `com.google.Chrome`        | `'rimZ'`     | `~/Library/Application Support/Google/Chrome/`        |
| Google Chrome Beta (SxS)     | `com.google.Chrome.beta`   | `'Late'`     | `~/Library/Application Support/Google/Chrome Beta/`   |
| Google Chrome Dev (SxS)      | `com.google.Chrome.dev`    | `'Prod'`     | `~/Library/Application Support/Google/Chrome Dev/`    |
| Google Chrome Canary         | `com.google.Chrome.canary` | `'Pipi'`     | `~/Library/Application Support/Google/Chrome Canary/` |

# Instructions to enable or disable Lacros

Google employees: See [go/lacros-dogfood](http://go/lacros-dogfood).

## Enabling Lacros (AKA Lacros only)
Navigate to `chrome://flags` on the (legacy) Ash browser.

1. Set `#lacros-availability-ignore` to "Enabled".
1. Click "Restart".
1. Set `#lacros-only` to "Enabled".
1. Pre M116: Set `#lacros-support` to "Enabled".
1. Pre M116: Set `#lacros-primary` to "Enabled".
1. Click "Restart".
1. The Chrome browser icon will now open the Lacros browser!
1. You can leave this mode at any time and return to your previous Chrome OS
   (Ash) environment and state (windows, tabs).

## How to tell if Lacros is enabled
Navigate to `chrome://version` and check the first line of the version
information. If "lacros" is shown somewhere after the version number, then
Lacros is enabled.

## Disabling Lacros
Navigate to `os://flags` (if using the Lacros browser), or `chrome://flags` on
the (legacy) Ash browser.

1. Set `#lacros-availability-ignore` to "Enabled".
1. Click "Restart".
1. Set `#lacros-support` to "Disabled" and click "Restart".
1. Device is returned to the original state with the legacy ChromeOS browser
   (Ash).

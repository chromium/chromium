# Linux Chromium Packages

Some Linux distributions package up Chromium for easy installation. Please note
that Chromium is not identical to Google Chrome -- see
[chromium_browser_vs_google_chrome.md](../chromium_browser_vs_google_chrome.md) --
and that distributions may (and actually do) make their own modifications.

TODO: Move away from tables.

| **Distro** | **Contact** | **URL for packages** | **URL for distro-specific patches** |
|:-----------|:------------|:---------------------|:------------------------------------|
| Ubuntu     | Olivier Tilloy `olivier.tilloy@canonical.com` | https://launchpad.net/ubuntu/+source/chromium-browser | https://code.launchpad.net/~chromium-team |
| Debian     | chromium@packages.debian.org | https://tracker.debian.org/pkg/chromium | https://sources.debian.org/patches/chromium/ |
| openSUSE   | https://bugzilla.opensuse.org | https://software.opensuse.org/package/chromium | https://build.opensuse.org/package/show/openSUSE:Factory/chromium |
| Arch       | Evangelos Foutras `evangelos@foutrelis.com` | http://www.archlinux.org/packages/extra/x86_64/chromium/ | https://github.com/archlinux/svntogit-packages/tree/master/chromium/trunk |
| Gentoo     | http://www.gentoo.org/proj/en/desktop/chromium/index.xml | http://packages.gentoo.org/package/www-client/chromium | https://gitweb.gentoo.org/proj/chromium-tools.git/ |
| ALT Linux  | Andrey Cherepanov (Андрей Черепанов) `cas@altlinux.org` | http://packages.altlinux.org/en/Sisyphus/srpms/chromium | http://git.altlinux.org/gears/c/chromium.git?a=tree |
| Mageia     | Dexter Morgan `dmorgan@mageia.org` | http://svnweb.mageia.org/packages/cauldron/chromium-browser-stable/current/SPECS/ | http://svnweb.mageia.org/packages/cauldron/chromium-browser-stable/current/SOURCES/ |
| NixOS      | aszlig `"^[0-9]+$"@regexmail.net` | https://search.nixos.org/packages?query=chromium | https://github.com/NixOS/nixpkgs/tree/master/pkgs/applications/networking/browsers/chromium |
| OpenMandriva | Bernhard Rosenkraenzer `bero@lindev.ch` | n/a | https://github.com/OpenMandrivaAssociation/chromium-browser-stable https://github.com/OpenMandrivaAssociation/chromium-browser-beta https://github.com/OpenMandrivaAssociation/chromium-browser-dev |
| Fedora     | Tom Callaway `tcallawa@redhat.com` | https://src.fedoraproject.org/rpms/chromium/ | https://src.fedoraproject.org/rpms/chromium/tree/rawhide |
| Yocto      | Raphael Kubo da Costa `raphael.kubo.da.costa@intel.com` | https://github.com/OSSystems/meta-browser | https://github.com/OSSystems/meta-browser/tree/master/meta-chromium/recipes-browser/chromium/files |
| Exherbo    | Timo Gurr `tgurr@exherbo.org` | https://git.exherbo.org/summer/packages/net-www/chromium-stable/ | https://git.exherbo.org/desktop.git/tree/packages/net-www/chromium-stable/files |

## Unofficial packages

Packages in this section are not part of the distro's official repositories.

| **Distro** | **Contact** | **URL for packages** | **URL for distro-specific patches** |
|:-----------|:------------|:---------------------|:------------------------------------|
| Slackware  | Eric Hameleers `alien@slackware.com` | http://www.slackware.com/~alien/slackbuilds/chromium/ | http://www.slackware.com/~alien/slackbuilds/chromium/ |

## Other Unixes

| **System** | **Contact** | **URL for packages** | **URL for patches** |
|:-----------|:------------|:---------------------|:--------------------|
| FreeBSD    | freebsd-chromium@freebsd.org | https://wiki.freebsd.org/Chromium | https://cgit.freebsd.org/ports/tree/www/chromium/files |
| OpenBSD    | Robert Nagy `robert@openbsd.org` | http://openports.se/www/chromium | https://cvsweb.openbsd.org/ports/www/chromium/patches/ |

## Updating the list

Are you packaging Chromium for a Linux distro? Is the information above out of
date? Please contact the folks in
[//build/linux/OWNERS](../../build/linux/OWNERS) with updates or
[contribute](../contributing.md) an update.

Before contacting, please note:

*   This is not the channel for technical support
*   The answer to questions about Linux distros not listed above is
    "We don't know"
*   Linux distros supported by Google Chrome are listed here:
    https://support.google.com/chrome/answer/95411

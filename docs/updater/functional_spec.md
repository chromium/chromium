# Chromium Updater Functional Specification

This is the functional specification for [Chromium Updater](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/).


[TOC]

## Elevation on Windows when a system application installer is executed

`UpdaterSetup.exe`, also known as the metainstaller, is typically downloaded
from the Internet and run by the user from a web browser. When this
metainstaller is executed, the browser launches it at
[medium integrity](https://docs.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control).
In order to be able to install a system application (i.e., an application
installed for all users on the system), this metainstaller needs to run with
administrative privileges.

To achieve this, the metainstaller detects this condition (running at medium but
trying to install a system app) early on, and re-launches itself at high
integrity. This will result in an
[UAC prompt](https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works)
on Windows.

## Dynamic Install Parameters

### `installdataindex`

`installdataindex` is one of the install parameters that can be specified for
first installs on the command line or via the
[metainstaller tag](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/tools/tag.py).

For example, here is a typical command line for the Updater on Windows:
```
UpdaterSetup.exe /install "appguid=YourAppID&appname=YourAppName&needsadmin=False&lang=en&installdataindex =verboselog"
```

In this case, the updater client sends the `installdataindex` of `verboselog` to
the update server.

This is how a [JSON](https://www.json.org/) request from the updater client may
look like:

```
{
   "request":{
      "@os":"win",
      "@updater":"updater",
      "acceptformat":"crx3",
      "app":[
         {
            "appid":"YourAppID",
            "data":[
               {
                  "index":"verboselog",
                  "name":"install"
               }
            ],
            "enabled":true,
            "installsource":"ondemand",
            "ping":{
               "r":-2
            },
            "updatecheck":{
               "sameversionupdate":true
            },
            "version":"0.1"
         }
      ],
      "arch":"x86",
      "dedup":"cr",
      "domainjoined":true,
      "hw":{
         "avx":true,
         "physmemory":32,
         "sse":true,
         "sse2":true,
         "sse3":true,
         "sse41":true,
         "sse42":true,
         "ssse3":true
      },
      "ismachine":false,
      "lang":"en-US",
      "nacl_arch":"x86-64",
      "os":{
         "arch":"x86_64",
         "platform":"Windows",
         "version":"10.0.19042.1586"
      },
      "prodversion":"101.0.4949.0",
      "protocol":"3.1",
      "requestid":"{6b417770-1f68-4d52-8843-356760c84d33}",
      "sessionid":"{37775211-4487-48d5-845d-35a1d71b03bc}",
      "updaterversion":"101.0.4949.0",
      "wow64":true
   }
}
```

The server retrieves the data corresponding to `installdataindex=verboselog`
and returns it back to the updater client.

This is how a JSON response from the update server may look like:

```
  "response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "data":[{
      "status":"ok",
      "name":"install",
      "index":"verboselog",
      "#text":"{\"logging\":{\"verbose\":true}}"
     }],
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "run":"UpdaterSetup.exe",
      "arguments":"--arg1 --arg2",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }
```

The updater client writes this data to a temporary file in the same directory as
the application installer. This is for security reasons, since writing the data
to the temp directory could potentially allow a man-in-the-middle attack.

The updater client provides the temporary file as a parameter to the application
installer.

Let's say, as shown above, that the update server responds with these example
file contents:
```
{"logging":{"verbose":true}}
```

The updater client will now create a temporary file, say `c:\my
path\temporaryfile.dat` (assuming the application installer is running from
`c:\my path\YesExe.exe`), with the following file contents:
```
\xEF\xBB\xBF{"logging":{"verbose":true}}
```

and then provide the file as a parameter to the application installer:
```
"c:\my path\YesExe.exe" --installerdata="c:\my path\temporaryfile.dat"
```

* Notice above that the temp file contents are prefixed with an UTF-8 Byte Order
Mark of `EF BB BF`.
* For MSI installers, a property will passed to the installer:
`INSTALLERDATA="pathtofile"`.
* For exe-based installers, as shown above, a command line parameter will be
passed to the installer: `--installerdata="pathtofile"`.
* For Mac installers, an environment variable will be set:
`INSTALLERDATA="pathtofile"`.
* Ownership of the temp file is the responsibility of the application installer.
The updater will not delete this file.
* This installerdata is not persisted anywhere else, and it is not sent as a
part of pings to the update server.

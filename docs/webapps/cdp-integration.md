# Instruction of using PWA via CDP


## Terms

[Chrome DevTools Protocol (i.e. CDP)](https://chromedevtools.github.io/devtools-protocol/) allows for tools to instrument, inspect, debug and profile Chromium, Chrome and other Blink-based browsers.

[Progressive Web App (i.e. PWA)](https://web.dev/explore/progressive-web-apps) are web apps built and enhanced with modern APIs to provide enhanced capabilities while still reaching any web user on any device with a single codebase.

CDP + PWA provides CDP to control the behavior of the browser to debug and test PWA.


## Demo

The preferred way of running CDP is to [use puppeteer](https://developer.chrome.com/docs/puppeteer), but any testing framework that can issue devtools commands will work. This document won’t cover too much detail about the puppeteer itself, but only focuses on how to use it to access PWA commands.


#### Prerequisites

The demo needs npm and nodejs, puppeteer is installed by npm. An apt-based installation script looks like.

```
sudo apt install npm nodejs
npm i puppeteer --save
```

Demo uses Chrome dev channel (or google-chrome-unstable in some contexts) on linux. Eventually the required changes will be released to the Chrome production (M128 or upper), and regular Chrome stable channel (or google-chrome in some contexts) can be used.


#### Important Caveat - uninstall when you are done!

This 'actually' installs the web app on your system - to ensure this is properly cleaned up after your test is complete, please make sure you uninstall the web app in the test!

TODO(https://crbug.com/339457135): Add ability to mock & clean up app installations as a command-line flag.


#### Basic Ideas

(May skip this section if you are familiar with puppeteer and CDP.)

Create a browser instance via puppeteer.launch,


```
const browser = await puppeteer.launch({
  // No need to use unstable once the PWA implementations roll to prod.
  executablePath:
      '/usr/bin/google-chrome-unstable',
  headless: false,
  args: ['--window-size=700,700', '--window-position=10,10'],
  // Use pipe to allow executing high privilege commands.
  pipe: true,
});
```


Create a browser CDP session,


```
const browserSession = await browser.target().createCDPSession();
```


Send CDP commands via CDP session. This function also logs the I/O to the console, but only the `session.send` matters.


```
async function send(session, msg, param) {
  if (session == null) {
    session = browserSession;
  }
  console.log('    >>> Sending: ', msg, ': ', JSON.stringify(param));
  let result, error;
  try {
    result = await session.send(msg, param);
  } catch (e) {
    error = e;
  }
  console.log('    <<< Response: ', trim(JSON.stringify(result)));
  console.log('    <<< Error: ', error);
}
```


Create a page CDP session,


```
async function current_page_session() {
  return (await browser.pages()).pop().createCDPSession();
}
```



#### Demo2.mjs

The demo installs a webapp https://developer.chrome.com/, launches it, inspects its states, opens it into its own app window, and uninstalls it.

The source code is at [cdp/demo2.mjs](cdp/demo2.mjs); run with the command
`nodejs docs/webapps/cdp/demo2.mjs`.

A video of running demo2.mjs on chrome canary channel on linux can be found at https://youtu.be/G4wmhSCXhH4.


#### Access via WebSocket

Though it’s less preferred, directly using WebSocket is also possible. It requires more effort to manage the I/O. More details won’t be discussed here, [cdp/demo.mjs](cdp/demo.mjs) is an example.


## Supported APIs

[See the source code.](https://source.chromium.org/search?q=domain%5CsPWA$%20f:browser_protocol.pdl%20-f:devtools-frontend&ssfr=1)


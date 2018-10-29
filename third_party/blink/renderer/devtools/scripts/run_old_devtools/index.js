const path = require('path');
const fs = require('fs');
const puppeteer = require('puppeteer');
const childProcess = require('child_process');
const CHROMIUM_DEFAULT_PATH = path.resolve(__dirname, '..', '..', '..', '..', '..', '..', 'out', 'Release', 'chrome');

(async function() {
  const targetRevision = process.argv[2];
  if (!targetRevision || String(parseInt(targetRevision, 10)) !== targetRevision) {
    console.log('The first argument should be a Chromium revision number, like 338865');
    return;
  }
  const info = await downloadChrome(targetRevision);
  childProcess.exec(`${info.executablePath} --remote-debugging-port=9227 --user-data-dir=user-data-dir about:blank`);
  const browser = await puppeteer.launch({
    executablePath: process.env.CHROMIUM_PATH || CHROMIUM_DEFAULT_PATH,
    dumpio: true,
    defaultViewport: null,
    ignoreDefaultArgs: true,
    args: puppeteer.defaultArgs({
      headless: false,
      args: ['--no-default-browser-check', `--custom-devtools-frontend=http://localhost:9227/devtools/`],
      userDataDir: 'debugging-user-data-dir'
    }).filter(x => !x.startsWith('--enable-automation'))
  });
  const page = (await browser.pages())[0];
  await page.goto('http://localhost:9227');
  const chromeVersion = await page.evaluate(async () => {
    const data = await fetch('json/version');
    const json = await data.json();
    const userAgent = json['User-Agent'];
    const start = userAgent.indexOf('Chrome/') + 'Chrome/'.length;
    return userAgent.substring(start, userAgent.indexOf(' ', start));
  });
  console.log(`Launched Chromium ${chromeVersion}`);
  await page.bringToFront();
  await page.waitForSelector('a');
  const realURL = await page.evaluate(() => document.querySelector('a').href);

  const url = `chrome-devtools://devtools/custom/${realURL.substring('http://localhost:9227/devtools/'.length)}&remoteVersion=${chromeVersion}`;

  await page.goto(url);
})();

async function downloadChrome(revision) {
  const fetcher = puppeteer.createBrowserFetcher();
  let progressBar = null;
  let lastDownloadedBytes = 0;
  return await fetcher.download(revision, (downloadedBytes, totalBytes) => {
    if (!progressBar) {
      const ProgressBar = require('progress');
      progressBar = new ProgressBar(`Downloading Chromium r${revision} - ${toMegabytes(totalBytes)} [:bar] :percent :etas `, {
        complete: '=',
        incomplete: ' ',
        width: 20,
        total: totalBytes,
      });
    }
    const delta = downloadedBytes - lastDownloadedBytes;
    lastDownloadedBytes = downloadedBytes;
    progressBar.tick(delta);
  });
}

function toMegabytes(bytes) {
  const mb = bytes / 1024 / 1024;
  return `${Math.round(mb * 10) / 10} Mb`;
}
const path = require('path');
const http = require('http');
const crypto = require('crypto');
const child_process = require('child_process');
const express = require('express');

const httpPort = 48080;
const wsPort = 48081;

const app = express();
app.use(express.static(__dirname));
http.createServer(app)
  .listen(httpPort, async err => {
    if (!err) {
      let qrvrProcess, chromeProcess;
      
      const u = process.argv.slice(2).find(arg => !/^--/.test(arg)) || `https://home.metachromium.com/`;
      const qrFlag = process.argv.slice(2).some(arg => arg === '--qr');
      const key = crypto.randomBytes(32).toString('hex');

      const _startQr = () => new Promise((accept, reject) => {
        qrvrProcess = child_process.fork(
          path.join(__dirname, 'child.js'),
          [
            wsPort,
          ],
          {
            stdio: 'pipe',
            cwd: path.dirname(require.resolve('qrvr')),
          }
        );
        qrvrProcess.on('exit', _exit);
        qrvrProcess.stdout.setEncoding('utf8');
        const _data = d => {
          if (/started/.test(d)) {
            qrvrProcess.stdout.removeListener('data', _data);
            accept();
          }
        };
        qrvrProcess.stdout.on('data', _data);
        qrvrProcess.stdout.pipe(process.stdout);
        qrvrProcess.stderr.pipe(process.stderr);
      });
      const _startChrome = () => {
        chromeProcess = child_process.spawn(
          './Chrome-bin/chrome.exe',
          [
            '--no-sandbox',
            '--disable-web-security',
            '--test-type',
            '--disable-xr-device-consent-prompt-for-testing',
            '--disable-gesture-requirement-for-presentation',
            '--use-fake-ui-for-media-stream',
            '--no-user-gesture-required',
            '--autoplay-policy=no-user-gesture-required',
            `--user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.129 Safari/537.36 Mchr/1.0 (${wsPort}, ${key})`,
            u,
          ],
          {
            shell: false,
          }
        );
        chromeProcess.on('exit', _exit);
      };
      function _exit() {
        qrvrProcess && qrvrProcess.kill();
        chromeProcess && chromeProcess.kill();
        process.kill(process.pid);
      }
      if (qrFlag) {
        await _startQr();
      }
      _startChrome();
    } else {
      console.warn(err.stack);
      process.exit(1);
    }
  });
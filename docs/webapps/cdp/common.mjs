import {stdin as input, stdout as output} from 'node:process';
import * as readline from 'node:readline/promises';
import puppeteer from 'puppeteer';

export const browser = await puppeteer.launch({
  // No need to use unstable once the PWA implementations roll to prod.
  executablePath:
      '/usr/bin/google-chrome-unstable',
  headless: false,
  args: ['--window-size=700,700', '--window-position=10,10'],
  // Use pipe to allow executing high priviledge commands.
  pipe: true,
});

const browserSession = await browser.target().createCDPSession();

const rl = readline.createInterface({input, output});

export async function waitfor_enter(msg) {
  console.log('============================================================');
  console.log('**** ', msg);
  await rl.question('', ans => {});
}

export async function trim(text) {
  const len = 200;
  if (!text || !text.length || text.length < len) {
    return text;
  }
  return text.substring(0, len) + '...';
}

export async function send(session, msg, param) {
  if (session == null) {
    session = browserSession;
  }
  console.log('\x1b[32m')
  console.log('    >>> Sending: ', msg, ': ', JSON.stringify(param));
  let result, error;
  try {
    result = await session.send(msg, param);
  } catch (e) {
    error = e;
  }
  console.log('    <<< Response: ', trim(JSON.stringify(result)));
  console.log('    <<< Error: ', error);
  console.log('\x1b[0m');
}

export async function current_page_session() {
  return (await browser.pages()).pop().createCDPSession();
}

export function shutdown() {
  rl.close();
  process.exit();
}

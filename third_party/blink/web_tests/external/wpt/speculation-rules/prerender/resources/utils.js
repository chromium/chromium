const STORE_URL = '/speculation-rules/prerender/resources/key-value-store.py';

function assertSpeculationRulesIsSupported() {
  assert_implements(
      'supports' in HTMLScriptElement,
      'HTMLScriptElement.supports is not supported');
  assert_implements(
      HTMLScriptElement.supports('speculationrules'),
      '<script type="speculationrules"> is not supported');
}

// Starts prerendering for `url`.
function startPrerendering(url) {
  // Adds <script type="speculationrules"> and specifies a prerender candidate
  // for the given URL.
  // TODO(https://crbug.com/1174978): <script type="speculationrules"> may not
  // start prerendering for some reason (e.g., resource limit). Implement a
  // WebDriver API to force prerendering.
  const script = document.createElement('script');
  script.type = 'speculationrules';
  script.text = `{"prerender": [{"source": "list", "urls": ["${url}"] }] }`;
  document.head.appendChild(script);
}

// Reads the value specified by `key` from the key-value store on the server.
async function readValueFromServer(key) {
  const serverUrl = `${STORE_URL}?key=${key}`;
  const response = await fetch(serverUrl);
  if (!response.ok)
    throw new Error('An error happened in the server');
  const value = await response.text();

  // The value is not stored in the server.
  if (value === "")
    return { status: false };

  return { status: true, value: value };
}

// Convenience wrapper around the above getter that will wait until a value is
// available on the server.
async function nextValueFromServer(key) {
  while (true) {
    // Fetches the test result from the server.
    const { status, value } = await readValueFromServer(key);
    if (!status) {
      // The test result has not been stored yet. Retry after a while.
      await new Promise(resolve => setTimeout(resolve, 100));
      continue;
    }

    return value;
  }
}

// Writes `value` for `key` in the key-value store on the server.
async function writeValueToServer(key, value) {
  const serverUrl = `${STORE_URL}?key=${key}&value=${value}`;
  await fetch(serverUrl);
}

// Loads the initiator page, and navigates to the prerendered page after it
// receives the 'readyToActivate' message.
function loadInitiatorPage() {
  // Used to communicate with the prerendering page.
  const prerenderChannel = new BroadcastChannel('prerender-channel');
  window.addEventListener('unload', () => {
    prerenderChannel.close();
  });

  // We need to wait for the 'readyToActivate' message before navigation
  // since the prerendering implementation in Chromium can only activate if the
  // response for the prerendering navigation has already been received and the
  // prerendering document was created.
  const readyToActivate = new Promise((resolve, reject) => {
    prerenderChannel.addEventListener('message', e => {
      if (e.data != 'readyToActivate')
        reject(`The initiator page receives an unsupported message: ${e.data}`);
      resolve(e.data);
    });
  });

  const url = new URL(document.URL);
  url.searchParams.append('prerendering', '');
  // Prerender a page that notifies the initiator page of the page's ready to be
  // activated via the 'readyToActivate'.
  startPrerendering(url.toString());

  // Navigate to the prerendered page after being informed.
  readyToActivate.then(() => {
    window.location = url.toString();
  }).catch(e => {
    const testChannel = new BroadcastChannel('test-channel');
    testChannel.postMessage(
        `Failed to navigate the prerendered page: ${e.toString()}`);
    testChannel.close();
    window.close();
  });
}

// Returns messages received from the given BroadcastChannel
// so that callers do not need to add their own event listeners.
// nextMessage() returns a promise which resolves with the next message.
//
// Usage:
//   const channel = new BroadcastChannel('channel-name');
//   const messageQueue = new BroadcastMessageQueue(channel);
//   const message1 = await messageQueue.nextMessage();
//   const message2 = await messageQueue.nextMessage();
//   message1 and message2 are the messages received.
class BroadcastMessageQueue {
  constructor(broadcastChannel) {
    this.messages = [];
    this.resolveFunctions = [];
    this.channel = broadcastChannel;
    this.channel.addEventListener('message', e => {
      if (this.resolveFunctions.length > 0) {
        const fn = this.resolveFunctions.shift();
        fn(e.data);
      } else {
        this.messages.push(e.data);
      }
    });
  }

  // Returns a promise that resolves with the next message from this queue.
  nextMessage() {
    return new Promise(resolve => {
      if (this.messages.length > 0)
        resolve(this.messages.shift())
      else
        this.resolveFunctions.push(resolve);
    });
  }
}

// Returns <iframe> element upon load.
function createFrame(url) {
  return new Promise(resolve => {
      const frame = document.createElement('iframe');
      frame.src = url;
      frame.onload = () => resolve(frame);
      document.body.appendChild(frame);
    });
}

class PrerenderChannel extends EventTarget {
  broadcastChannel = null;

  constructor(uid, name) {
    super();
    this.broadcastChannel = new BroadcastChannel(`${uid}-${name}`);
    this.broadcastChannel.addEventListener('message', e => {
      this.dispatchEvent(new CustomEvent('message', {detail: e.data}));
    });
  }

  postMessage(message) {
    this.broadcastChannel.postMessage(message);
  }

  close() {
    this.broadcastChannel.close();
  }
};

async function create_prerendered_page(t) {
  const uuid = token();
  new PrerenderChannel(uuid, 'log').addEventListener('message', message => {
    // Calling it with ['log'] to avoid lint issue. This log should be used for debugging
    // the prerendered context, not testing.
    if(window.console)
      console['log']('[From Prerendered]', ...message.detail);
  });

  const execChannel = new PrerenderChannel(uuid, 'exec');
  const initChannel = new PrerenderChannel(uuid, 'initiator');
  const exec = (func, args = []) => {
      const receiver = token();
      execChannel.postMessage({receiver, fn: func.toString(), args});
      return new Promise((resolve, reject) => {
        const channel = new PrerenderChannel(uuid, receiver);
        channel.addEventListener('message', ({detail}) => {
          channel.close();
          if (detail.error)
            reject(detail.error)
          else
            resolve(detail.result);
        });
      })
    };

  window.open(`/speculation-rules/prerender/resources/eval-init.html?uuid=${uuid}`, '_blank', 'noopener');
  t.add_cleanup(() => initChannel.postMessage('close'));
  t.add_cleanup(() => exec(() => window.close()));
  await new Promise(resolve => {
    const channel = new PrerenderChannel(uuid, 'ready');
    channel.addEventListener('message', () => {
      channel.close();
      resolve();
    });
  });

  async function activate() {
    const prerendering = exec(() => new Promise(resolve =>
      document.addEventListener('prerenderingchange', () => {
        resolve(document.prerendering);
      })));

    initChannel.postMessage('activate');
    if (await prerendering)
      throw new Error('Should not be prerendering at this point')
  }

  return {
    exec,
    activate
  };
}


function test_prerender_restricted(fn, expected, label) {
  promise_test(async t => {
    const {exec} = await create_prerendered_page(t);
    let result = null;
    try {
      await exec(fn);
      result = "OK";
    } catch (e) {
      result = e.name;
    }

    assert_equals(result, expected);
  }, label);
}

function test_prerender_defer(fn, label) {
  promise_test(async t => {
    const {exec, activate} = await create_prerendered_page(t);
    let activated = false;
    const deferred = exec(fn);

    const post = new Promise(resolve =>
      deferred.then(result => {
        assert_true(activated, "Deferred operation should occur only after activation");
        resolve(result);
      }));

    await new Promise(resolve => t.step_timeout(resolve, 100));
    await activate();
    activated = true;
    await post;
  }, label);
}
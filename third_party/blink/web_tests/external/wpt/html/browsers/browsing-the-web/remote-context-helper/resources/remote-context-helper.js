'use strict';

// Requires:
// - /common/dispatcher/dispatcher.js
// - /common/utils.js
// - /common/get-host-info.sub.js if automagic conversion of origin names to
// URLs is used.

/**
 * This provides a more friendly interface to remote contexts in dispatches.js.
 * The goal is to make it easy to write multi-window/-frame/-worker tests where
 * the logic is entirely in 1 test file and there is no need to check in any
 * other file (although it is often helpful to check in files of JS helper
 * functions that are shared across remote context).
 *
 * So for example, to test that history traversal works, we create a new window,
 * navigate it to a new document, go back and then go forward.
 *
 * @example
 * promise_test(async t => {
 *   const rcHelper = new RemoteContextHelper();
 *   const rc1 = await rcHelper.addWindow();
 *   const rc2 = await rc1.navigateToNew();
 *   assert_equals(await rc2.executeScript(() => 'here'), 'here', 'rc2 is live');
 *   rc2.historyBack();
 *   assert_equals(await rc1.executeScript(() => 'here'), 'here', 'rc1 is live');
 *   rc1.historyForward();
 *   assert_equals(await rc2.executeScript(() => 'here'), 'here', 'rc2 is live');
 * });
 *
 * Note on the correspondence between remote contexts and
 * `RemoteContextWrapper`s. A remote context is entirely determined by its URL.
 * So navigating away from one and then back again will result in a remote
 * context that can be controlled by the same `RemoteContextWrapper` instance
 * before and after navigation. Messages sent to a remote context while it is
 * destroyed or in BFCache will be queued and processed if that that URL is
 * navigated back to.
 *
 * Navigation:
 * This framework does not keep track of the history of the frame tree and so it
 * is up to the test script to keep track of what remote contexts are currently
 * active and to keep references to the corresponding `RemoteContextWrapper`s.
 *
 * Any action that leads to navigation in the remote context must be executed
 * using
 * @see RemoteContextWrapper.navigate.
 */

{
  const RESOURCES_PATH =
      '/html/browsers/browsing-the-web/remote-context-helper/resources';
  const WINDOW_EXECUTOR_PATH = `${RESOURCES_PATH}/executor.html`;
  const WORKER_EXECUTOR_PATH = `${RESOURCES_PATH}/executor-worker.js`;

  /**
   * Turns a string into an origin. If `origin` is null this will return the
   * current document's origin. If `origin` contains not '/', this will attempt
   * to use it as an index in `get_host_info()`. Otherwise returns the input
   * origin.
   * @private
   * @param {string|null} origin The input origin.
   * @return {string|null} The output origin.
   * @throws {RangeError} is `origin` cannot be found in
   *     `get_host_info()`.
   */
  function finalizeOrigin(origin) {
    if (!origin) {
      return location.origin;
    }
    if (origin.indexOf('/') == -1) {
      const origins = get_host_info();
      if (origin in origins) {
        return origins[origin];
      } else {
        throw new RangeError(
            `${origin} is not a key in the get_host_info() object`);
      }
    }
    return origin;
  }

  /**
   * @private
   * @param {string} url
   * @returns {string} Absolute url using `location` as the base.
   */
  function makeAbsolute(url) {
    return new URL(url, location).toString();
  }

  /**
   * Represents a configuration for a remote context executor.
   */
  class RemoteContextConfig {
    /**
     * @param {string} origin A URL or a key in `get_host_info()`. @see finalizeOrigin for how origins are handled.
     * @param {string[]} scripts  A list of script URLs. The current document
     *     will be used as the base for relative URLs.
     * @param {[string, string][]} headers  A list of pairs of name and value.
     *     The executor will be served with these headers set.
     * @param {string} startOn If supplied, the executor will start when this
     *     event occurs, e.g. "pageshow",
     *  (@see window.addEventListener). This only makes sense for window-based executors, not worker-based.
     *
     */
    constructor(
        {origin = null, scripts = [], headers = [], startOn = null} = {}) {
      this.origin = origin;
      this.scripts = scripts;
      this.headers = headers;
      this.startOn = startOn;
    }

    /**
     * If `config` is not already a `RemoteContextConfig`, one is constructed
     * using `config`.
     * @private
     * @param {object} config
     * @returns
     */
    static ensure(config) {
      if (!config) {
        return DEFAULT_CONTEXT_CONFIG;
      }
      return new RemoteContextConfig(config);
    }

    /**
     * Merges `this` with another `RemoteContextConfig` and to give a new
     * `RemoteContextConfig`. `origin` is replaced by the other if present,
     * `headers` and `scripts` are concatenated with `this`'s coming first.
     * @param {RemoteContextConfig} extraConfig
     * @returns {RemoteContextConfig}
     */
    merged(extraConfig) {
      let origin = this.origin;
      if (extraConfig.origin) {
        origin = extraConfig.origin;
      }
      let startOn = this.startOn;
      if (extraConfig.startOn !== null) {
        startOn = extraConfig.startOn;
      }
      const headers = this.headers.concat(extraConfig.headers);
      const scripts = this.scripts.concat(extraConfig.scripts);
      return new RemoteContextConfig({
        origin: origin,
        headers: headers,
        scripts: scripts,
        startOn: startOn,
      });
    }
  }

  /**
   * The default `RemoteContextConfig` to use if none is supplied. It has no
   * origin, headers or scripts.
   * @constant {RemoteContextConfig}
   */
  const DEFAULT_CONTEXT_CONFIG = new RemoteContextConfig();

  /**
   * This class represents a configuration for creating remote contexts. This is
   * the entry-point
   * for creating remote contexts, providing @see addWindow .
   */
  class RemoteContextHelper {
    /**
     * @param {RemoteContextConfig|object} config The configuration
     *     for this remote context.
     */
    constructor(config = null) {
      this.config = RemoteContextConfig.ensure(config);
    }

    /**
     * Creates a new remote context and returns a `RemoteContextWrapper` giving
     * access to it.
     * @private
     * @param {(url: string) => RemoteContext} executorCreator A function that
     *     takes a URL and returns a context, e.g. an iframe or window.
     * @param {RemoteContextConfig|object|null} extraConfig If
     *     supplied, extra configuration for this remote context to be merged
     * with `this`'s existing config. If it's not a `RemoteContextConfig`, it
     * will be used to construct a new one.
     * @returns {RemoteContextWrapper}
     */
    async createContext({
      executorCreator: executorCreator,
      extraConfig = null,
      isWorker = false,
    }) {
      const config =
          this.config.merged(RemoteContextConfig.ensure(extraConfig));

      const origin = finalizeOrigin(config.origin);
      const url = new URL(
          isWorker ? WORKER_EXECUTOR_PATH : WINDOW_EXECUTOR_PATH, origin);

      // UUID is needed for executor.
      const uuid = token();
      url.searchParams.append('uuid', uuid);

      if (config.headers) {
        addHeaders(url, config.headers);
      }
      for (const script of config.scripts) {
        url.searchParams.append('script', makeAbsolute(script));
      }

      if (config.startOn) {
        url.searchParams.append('startOn', config.startOn);
      }

      executorCreator(url);
      return new RemoteContextWrapper(new RemoteContext(uuid), this);
    }

    /**
     * Creates a window with a remote context. @see createContext for
     * @param {RemoteContextConfig|object} extraConfig Will be
     *     merged with `this`'s config.
     * @param {string} options.target Passed to `window.open` as the 2nd
     *     argument
     * @param {string} options.features Passed to `window.open` as the 3rd
     *     argument
     * @returns {RemoteContextWrapper}
     */
    addWindow(extraConfig = null, options = {
      target: null,
      features: null,
    }) {
      return this.createContext({
        executorCreator: windowExecutorCreator(options),
        extraConfig: extraConfig,
      });
    }
  }
  // Export this class.
  self.RemoteContextHelper = RemoteContextHelper;

  /**
   * Attaches header to the URL. See
   * https://web-platform-tests.org/writing-tests/server-pipes.html#headers
   * @param {string} url the URL to which headers should be attached.
   * @param {[[string, string]]} headers a list of pairs of head-name,
   *     header-value.
   */
  function addHeaders(url, headers) {
    function escape(s) {
      return s.replace('(', '\\(').replace(')', '\\)');
    }
    const formattedHeaders = headers.map((header) => {
      return `header(${escape(header[0])}, ${escape(header[1])})`;
    });
    url.searchParams.append('pipe', formattedHeaders.join('|'));
  }

  function windowExecutorCreator({target, features}) {
    if (!target) {
      target = '_blank';
    }
    return url => {
      window.open(url, target, features);
    };
  }

  function elementExecutorCreator(
      remoteContextWrapper, elementName, attributes) {
    return url => {
      remoteContextWrapper.executeScript((url, elementName, attributes) => {
        const el = document.createElement(elementName);
        for (const attribute in attributes) {
          el.setAttribute(attribute, attributes[attribute]);
        }
        el.src = url;
        document.body.appendChild(el);
      }, [url, elementName, attributes]);
    };
  }

  function workerExecutorCreator() {
    return url => {
      new Worker(url);
    };
  }

  function navigateExecutorCreator(remoteContextWrapper) {
    return url => {
      remoteContextWrapper.navigate((url) => {
        window.location = url;
      }, [url]);
    };
  }

  /**
   * This class represents a remote context running an executor (a
   * window/frame/worker that can receive commands). It is the interface for
   * scripts to control remote contexts.
   *
   * Instances are returned when new remote contexts are created (e.g.
   * `addFrame` or `navigateToNew`).
   */
  class RemoteContextWrapper {
    /**
     * This should only be constructed by `RemoteContextHelper`.
     * @private
     */
    constructor(context, helper) {
      this.context = context;
      this.helper = helper;
    }

    /**
     * Executes a script in the remote context.
     * @param {function} fn The script to execute.
     * @param {any[]} args An array of arguments to pass to the script.
     * @returns {any} The return value of the script (after being serialized and
     *     deserialized).
     */
    async executeScript(fn, args) {
      return this.context.execute_script(fn, args);
    }

    /**
     * Adds a string of HTML to the executor's document.
     * @param {string} html
     * @returns
     */
    async addHtml(html) {
      return this.executeScript((htmlSource) => {
        document.body.insertAdjacentHTML('beforebegin', htmlSource);
      }, [html]);
    }

    /**
     * Adds scripts to the executor's document.
     * @param {string[]} urls A list of URLs. URLs are relative to the current
     *     document.
     * @returns
     */
    async addScripts(urls) {
      if (!urls) {
        return [];
      }
      return this.executeScript(urls => {
        return addScripts(urls);
      }, [urls.map(makeAbsolute)]);
    }

    /**
     * Adds an iframe to the current document.
     * @param {RemoteContextConfig} extraConfig
     * @param {[string, string][]} options.attributes A list of pairs of strings
     *     of attribute name and value these will be set on the iframe element
     *     when added to the document.
     * @returns {RemoteContextWrapper} The remote context.
     */
    addIframe(extraConfig = null, attributes = {}) {
      return this.helper.createContext({
        executorCreator: elementExecutorCreator(this, 'iframe', attributes),
        extraConfig: extraConfig,
      });
    }

    /**
     * Adds a dedicated worker to the current document.
     * @param {RemoteContextConfig} extraConfig
     * @returns {RemoteContextWrapper} The remote context.
     */
    addWorker(extraConfig = null) {
      return this.helper.createContext({
        executorCreator: workerExecutorCreator(),
        extraConfig: extraConfig,
        isWorker: true,
      });
    }

    /**
     * Executes a script in the remote context that will perform a navigation.
     * To do this safely, we must suspend the executor and wait for that to
     * complete before executing. This ensures that all outstanding requests are
     * completed and no more can start. It also ensures that the executor will
     * restart if the page goes into BFCache or it was a same-document
     * navigation. It does not return a value.
     *
     * NOTE: We cannot monitor whether and what navigations are happening. The
     * logic has been made as robust as possible but is not fool-proof.
     *
     * Foolproof rule:
     * - The script must perform exactly one navigation.
     * - If that navigation is a same-document history navigation, you must
     * `await` the result of `waitUntilLocationIs`.
     *
     * More complex rules:
     * - The script must perform a navigation. If it performs no navigation,
     *   the remote context will be left in the suspended state.
     * - If the script performs a direct same-document navigation, it is not
     * necessary to use this function but it will work as long as it is the only
     *   navigation performed.
     * - If the script performs a same-document history navigation, you must
     * `await` the result of `waitUntilLocationIs`.
     *
     * @param {function} fn The script to execute.
     * @param {any[]} args An array of arguments to pass to the script.
     */
    navigate(fn, args) {
      this.executeScript((fnText, args) => {
        executeScriptToNavigate(fnText, args);
      }, [fn.toString(), args]);
    }

    /**
     * Navigates the context to a new document running an executor.
     * @param {RemoteContextConfig} extraConfig
     * @returns {RemoteContextWrapper} The remote context.
     */
    async navigateToNew(extraConfig = null) {
      return this.helper.createContext({
        executorCreator: navigateExecutorCreator(this),
        extraConfig: extraConfig,
      });
    }

    //////////////////////////////////////
    // Navigation Helpers.
    //
    // It is up to the test script to know which remote context will be
    // navigated to and which `RemoteContextWrapper` should be used after
    // navigation.
    //
    // NOTE: For a same-document history navigation, the caller use `await` a
    // call to `waitUntilLocationIs` in order to know that the navigation has
    // completed. For convenience the method below can return the promise to
    // wait on, if passed the expected location.

    async waitUntilLocationIs(expectedLocation) {
      return this.executeScript(async (expectedLocation) => {
        if (location == expectedLocation) {
          return;
        }

        // Wait until the location updates to the expected one.
        await new Promise(resolve => {
          const listener = addEventListener('hashchange', (event) => {
            if (event.newURL == expectedLocation) {
              removeEventListener(listener);
              resolve();
            }
          });
        });
      }, [expectedLocation]);
    }

    /**
     * Performs a history traversal.
     * @param {integer} n How many steps to traverse. @see history.go
     * @param {string} expectedLocation If supplied will be passed to @see waitUntilLocationIs.
     * @returns The return value of `waitUntilLocationIs` or nothing.
     */
    async historyGo(n, expectedLocation = null) {
      this.navigate((n) => {
        history.go(n);
      }, [n]);
      if (expectedLocation) {
        return this.waitUntilLocationIs(expectedLocation);
      }
    }

    /**
     * Performs a history traversal back.
     * @param {string} expectedLocation If supplied will be passed to @see waitUntilLocationIs.
     * @returns The return value of `waitUntilLocationIs` or nothing.
     */
    async historyBack(expectedLocation = null) {
      this.navigate(() => {
        history.back();
      });
      if (expectedLocation) {
        return this.waitUntilLocationIs(expectedLocation);
      }
    }

    /**
     * Performs a history traversal back.
     * @param {string} expectedLocation If supplied will be passed to @see waitUntilLocationIs.
     * @returns The return value of `waitUntilLocationIs` or nothing.
     */
    async historyForward(expectedLocation = null) {
      this.navigate(() => {
        history.forward();
      });
      if (expectedLocation) {
        return this.waitUntilLocationIs(expectedLocation);
      }
    }
  }
}

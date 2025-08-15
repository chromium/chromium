// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns the import map object for the components found in `third_party`.
 * Use this function to dynamically inject the import map into HTML files.
 * @return {Object} The import map object
 */
export function getImportMap() {
    return {
        "imports": {
            "tslib": "/tslib/tslib.js",
            "lit": "/lit/index.js",
            "lit/": "/lit/",
            "@lit/reactive-element":
                "/@lit/reactive-element/reactive-element.js",
            "@lit/reactive-element/": "/@lit/reactive-element/",
            "lit-element": "/lit-element/index.js",
            "lit-element/": "/lit-element/",
            "lit-html": "/lit-html/lit-html.js",
            "lit-html/": "/lit-html/",
            "@material/web/": "/@material/web/",
            "@material/mwc-base/": "/@material/mwc-base/",
            "@lit/task": "/@lit/task/index.js",
            "@lit/task/": "/@lit/task/"
        }
    };
}

/**
 * Dynamically creates and injects an import map script element into the
 * document head. Should be called before any ES6 module imports.
 */
export function injectImportMap() {
    const importMapScript = document.createElement("script");
    importMapScript.type = "importmap";
    importMapScript.textContent = JSON.stringify(getImportMap());
    document.head.appendChild(importMapScript);
}

const COMPONENTS = {
    "md-assist-chip": "chips/assist-chip.js",
    "md-branded-fab": "fab/branded-fab.js",
    "md-checkbox": "checkbox/checkbox.js",
    "md-chip-set": "chips/chip-set.js",
    "md-circular-progress": "progress/circular-progress.js",
    "md-dialog": "dialog/dialog.js",
    "md-elevated-card": "labs/card/elevated-card.js",
    "md-fab": "fab/fab.js",
    "md-filled-button": "button/filled-button.js",
    "md-filled-card": "labs/card/filled-card.js",
    "md-filled-icon-button": "iconbutton/filled-icon-button.js",
    "md-filled-select": "select/filled-select.js",
    "md-filled-text-field": "textfield/filled-text-field.js",
    "md-filled-tonal-icon-button": "iconbutton/filled-tonal-icon-button.js",
    "md-filter-chip": "chips/filter-chip.js",
    "md-icon": "icon/icon.js",
    "md-icon-button": "iconbutton/icon-button.js",
    "md-input-chip": "chips/input-chip.js",
    "md-linear-progress": "progress/linear-progress.js",
    "md-list": "list/list.js",
    "md-list-item": "list/list-item.js",
    "md-menu": "menu/menu.js",
    "md-menu-item": "menu/menu-item.js",
    "md-outlined-button": "button/outlined-button.js",
    "md-outlined-card": "labs/card/outlined-card.js",
    "md-outlined-icon-button": "iconbutton/outlined-icon-button.js",
    "md-outlined-select": "select/outlined-select.js",
    "md-outlined-text-field": "textfield/outlined-text-field.js",
    "md-primary-tab": "tabs/primary-tab.js",
    "md-radio": "radio/radio.js",
    "md-secondary-tab": "tabs/secondary-tab.js",
    "md-select-option": "select/select-option.js",
    "md-slider": "slider/slider.js",
    "md-sub-menu": "menu/sub-menu.js",
    "md-suggestion-chip": "chips/suggestion-chip.js",
    "md-switch": "switch/switch.js",
    "md-tabs": "tabs/tabs.js",
    "md-text-button": "button/text-button.js"
};

/**
 * Pre-fetches component modules to speed up loading.
 * @param {string[]} components - An array of component tag names.
 */
function prefetchComponents(components) {
    components.forEach(name => {
        const link = document.createElement("link");
        link.rel = "modulepreload";
        link.href = `/@material/web/${COMPONENTS[name]}`;
        document.head.appendChild(link);
    });
}

/**
 * Dynamically imports the required Material Web Component modules.
 * @param {string[]} components - An array of component tag names.
 * @return {Promise<any[]>} A promise that resolves when all modules are
 * loaded.
 */
function loadComponents(components) {
    prefetchComponents(components);
    return Promise.all(components.map(name =>
        import(`/@material/web/${COMPONENTS[name]}`)
    ));
}

/**
 * Returns a promise that resolves when all CSS animations and transitions on
 * an element and its descendants have finished.
 * @param {HTMLElement} element The element to monitor.
 * @return {Promise<any[]>}
 */
function animationsFinished(element) {
  return Promise.all(
    element.getAnimations({ subtree: true }).map(animation =>
         animation.finished)
  );
}

/**
 * Awaits for a Lit element to be fully stable by checking its update cycle
 * and waiting for any associated animations to complete.
 * @param {HTMLElement} element The component element to wait for.
 */
export async function waitForStable(element) {
  await element.updateComplete;
  await animationsFinished(element);
  await new Promise(resolve => requestAnimationFrame(resolve));
  await element.updateComplete;

  const tagName = element.tagName;
  // Components with complex state changes: Need double updateComplete with
  // timeout to ensure all cascading DOM updates complete before tree dump.
  if (tagName === "MD-DIALOG" || tagName === "MD-LIST" ||
      tagName === "MD-FAB" || tagName === "MD-MENU" ||
      tagName === "MD-FILTER-CHIP" || tagName === "MD-CHIP-SET" ||
      tagName === "MD-FILLED-TEXT-FIELD" ||
      tagName === "MD-OUTLINED-TEXT-FIELD") {
    await element.updateComplete;
    await new Promise(resolve => setTimeout(resolve, 250));
    await element.updateComplete;

    // Dialogs are flaky on LSan/ASan.
    if (tagName === "MD-DIALOG") {
      document.body.offsetHeight; // Force layout
      await new Promise(resolve => requestAnimationFrame(resolve));
      await new Promise(resolve => requestAnimationFrame(resolve));
    }
  }

  // Form controls: Layout stability via forced reflow, single updateComplete
  // is sufficient to know we've reached the final state and can dump the tree.
  if (tagName === "MD-CHECKBOX" || tagName === "MD-RADIO" ||
      tagName === "MD-SWITCH" || tagName === "MD-FILLED-SELECT" ||
      tagName === "MD-OUTLINED-SELECT") {
    await new Promise(resolve => requestAnimationFrame(resolve));
    document.body.offsetHeight; // Force layout
    await new Promise(resolve => requestAnimationFrame(resolve));
    await element.updateComplete;
  }
}

/**
 * Waits for DOM tree to be stable by polling until the tree
 * structure stops changing.
 */
function waitForStableDOMTree() {
    return new Promise((resolve) => {
        let lastTreeString = "";
        let stableCount = 0;
        const requiredStableCount = 3;

        function checkTree() {
            document.body.offsetHeight;

            const currentTreeString = Array.from(
                document.querySelectorAll("*"))
                .filter(el => el.isConnected && el.offsetParent !== null)
                .map(el => `${el.tagName}:${el.textContent?.trim() || ""}`)
                .join("|");

            if (currentTreeString === lastTreeString) {
                stableCount++;
                if (stableCount >= requiredStableCount) {
                    resolve();
                    return;
                }
            } else {
                stableCount = 0;
                lastTreeString = currentTreeString;
            }

            requestAnimationFrame(checkTree);
        }

        checkTree();
    });
}

/**
 * Waits for icon fonts to be fully loaded and rendered.
 */
function waitForIconFonts() {
    return new Promise((resolve) => {
        const iconElements = document.querySelectorAll("md-icon");
        if (iconElements.length === 0) {
            resolve();
            return;
        }

        let stableCount = 0;
        const requiredStableCount = 5;
        let lastIconSizes = "";

        function checkIconSizes() {
            const currentIconSizes = Array.from(iconElements)
                .map(icon => `${icon.offsetWidth}x${icon.offsetHeight}`)
                .join("|");

            if (currentIconSizes === lastIconSizes &&
                currentIconSizes !== "0x0|".repeat(iconElements.length)) {
                stableCount++;
                if (stableCount >= requiredStableCount) {
                    resolve();
                    return;
                }
            } else {
                stableCount = 0;
                lastIconSizes = currentIconSizes;
            }

            requestAnimationFrame(checkIconSizes);
        }

        checkIconSizes();
    });
}

/**
 * Loads a list of components and waits for them to be fully defined, rendered,
 * and stable (including animations) before executing a callback.
 * @param {string[]} components - An array of component tag names to load.
 * @param {Function} callback - The function to execute once all components are
 * stable.
 */
export function loadAndWaitForReady(components, callback) {
    const targetDiv = document.getElementById("status");

    // Hide the status div initially.
    targetDiv.style.visibility = "hidden";
    // Wait for fonts because components with icons need them.
    Promise.all([
        loadComponents(components),
        document.fonts.ready.catch(() => {
            console.warn("Font loading failed, continuing without fonts");
        })
    ])
        .then(() => {
            return Promise.all(components.map(name =>
                customElements.whenDefined(name)));
        })
        .then(() => {
            return new Promise(resolve => setTimeout(resolve, 200));
        })
        .then(() => {
            // Show the status div only when everything is ready.
            targetDiv.style.visibility = "visible";
            callback();
        })
        .then(() => {
            return new Promise((resolve) => {
                const checkReady = () => {
                    if (targetDiv.getAttribute("aria-label") === "Ready") {
                        resolve();
                    } else {
                        setTimeout(checkReady, 50);
                    }
                };
                checkReady();
            });
        })
        .then(() => {
            const elements = targetDiv.querySelectorAll(components.join(","));
            return Promise.all(Array.from(elements).map(el =>
                el ? waitForStable(el) : Promise.resolve()));
        })
        .then(() => {
            return waitForIconFonts();
        })
        .then(() => {
            return waitForStableDOMTree();
        })
        .catch(error => {
            console.error("Failed to load or stabilize components:", error);
            targetDiv.style.visibility = "visible";
            callback();
        });
}


/**
 * Sets up the global go function that the test framework expects.
 * This is needed so that we can do additional checks to be sure we're ready
 * to run a given Material Design test (which we might not be due to loading
 * components, waiting for animations, etc.).
 */
export function setupEventTestRunner() {
    window.go = function() {
        if (typeof window.go_passes === "undefined") {
            console.error("go_passes not found in global scope");
            return false;
        }

        if (typeof window.current_pass === "undefined") {
            window.current_pass = 0;
        }

        function checkReady() {
            const statusDiv = document.getElementById("status");
            document.body.offsetHeight; // Force layout
            const isReady = statusDiv &&
                           statusDiv.getAttribute("aria-label") === "Ready" &&
                           statusDiv.style.visibility === "visible" &&
                           statusDiv.offsetParent !== null;

            if (isReady) {
                return new Promise(resolve => {
                    setTimeout(async () => {
                        if (window.current_pass < window.go_passes.length) {
                            try {
                                const result =
                                    window.go_passes[window.current_pass++]
                                        .call();
                                if (result &&
                                    typeof result.then === "function") {
                                    await result;
                                }

                                const statusDiv =
                                    document.getElementById("status");
                                if (statusDiv) {
                                    const components =
                                        statusDiv.querySelectorAll(
                                        "md-filled-select, " +
                                        "md-outlined-select, " +
                                        "md-filled-text-field, " +
                                        "md-outlined-text-field, " +
                                        "md-checkbox, md-radio, md-switch"
                                    );
                                    for (const component of components) {
                                        await waitForStable(component);
                                    }
                                }
                            } catch (error) {
                                console.error(`Error in go_passes[${window.current_pass - 1}]:`, error);
                            }
                        }

                        resolve(window.current_pass < window.go_passes.length);
                    }, 50);
                });
            } else {
                return new Promise(resolve => {
                    setTimeout(() => {
                        resolve(checkReady());
                    }, 50);
                });
            }
        }
        return checkReady();
    };
}

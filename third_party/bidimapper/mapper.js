(function () {
    'use strict';

    function mitt(n){return {all:n=n||new Map,on:function(t,e){var i=n.get(t);i?i.push(e):n.set(t,[e]);},off:function(t,e){var i=n.get(t);i&&(e?i.splice(i.indexOf(e)>>>0,1):n.set(t,[]));},emit:function(t,e){var i=n.get(t);i&&i.slice().map(function(n){n(e);}),(i=n.get("*"))&&i.slice().map(function(n){n(t,e);});}}}

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class EventEmitter {
        #emitter = mitt();
        on(type, handler) {
            this.#emitter.on(type, handler);
            return this;
        }
        /**
         * Like `on` but the listener will only be fired once and then it will be removed.
         * @param event The event you'd like to listen to
         * @param handler The handler function to run when the event occurs
         * @return `this` to enable chaining method calls.
         */
        once(event, handler) {
            const onceHandler = (eventData) => {
                handler(eventData);
                this.off(event, onceHandler);
            };
            return this.on(event, onceHandler);
        }
        off(type, handler) {
            this.#emitter.off(type, handler);
            return this;
        }
        /**
         * Emits an event and call any associated listeners.
         *
         * @param event The event to emit.
         * @param eventData Any data to emit with the event.
         * @return `true` if there are any listeners, `false` otherwise.
         */
        emit(event, eventData) {
            this.#emitter.emit(event, eventData);
        }
        /**
         * Removes all listeners. If given an event argument, it will remove only
         * listeners for that event.
         * @param event - the event to remove listeners for.
         * @returns `this` to enable you to chain method calls.
         */
        removeAllListeners(event) {
            if (event) {
                this.#emitter.all.delete(event);
            }
            else {
                this.#emitter.all.clear();
            }
            return this;
        }
    }

    /**
     * Copyright 2021 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    var LogType;
    (function (LogType) {
        // keep-sorted start
        LogType["bidi"] = "bidi";
        LogType["cdp"] = "cdp";
        LogType["debug"] = "debug";
        LogType["debugError"] = "debug:error";
        LogType["debugInfo"] = "debug:info";
        // keep-sorted end
    })(LogType || (LogType = {}));

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    var _a$6;
    class ProcessingQueue {
        static LOGGER_PREFIX = `${LogType.debug}:queue`;
        #logger;
        #processor;
        #queue = [];
        // Flag to keep only 1 active processor.
        #isProcessing = false;
        constructor(processor, logger) {
            this.#processor = processor;
            this.#logger = logger;
        }
        add(entry, name) {
            this.#queue.push([entry, name]);
            // No need in waiting. Just initialize processor if needed.
            void this.#processIfNeeded();
        }
        async #processIfNeeded() {
            if (this.#isProcessing) {
                return;
            }
            this.#isProcessing = true;
            while (this.#queue.length > 0) {
                const arrayEntry = this.#queue.shift();
                if (!arrayEntry) {
                    continue;
                }
                const [entryPromise, name] = arrayEntry;
                this.#logger?.(_a$6.LOGGER_PREFIX, 'Processing event:', name);
                await entryPromise
                    .then((entry) => {
                    if (entry.kind === 'error') {
                        this.#logger?.(LogType.debugError, 'Event threw before sending:', entry.error.message, entry.error.stack);
                        return;
                    }
                    return this.#processor(entry.value);
                })
                    .catch((error) => {
                    this.#logger?.(LogType.debugError, 'Event was not processed:', error?.message);
                });
            }
            this.#isProcessing = false;
        }
    }
    _a$6 = ProcessingQueue;

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    // keep-sorted end
    var BiDiModule;
    (function (BiDiModule) {
        // keep-sorted start
        BiDiModule["Bluetooth"] = "bluetooth";
        BiDiModule["Browser"] = "browser";
        BiDiModule["BrowsingContext"] = "browsingContext";
        BiDiModule["Cdp"] = "cdp";
        BiDiModule["Input"] = "input";
        BiDiModule["Log"] = "log";
        BiDiModule["Network"] = "network";
        BiDiModule["Script"] = "script";
        BiDiModule["Session"] = "session";
        // keep-sorted end
    })(BiDiModule || (BiDiModule = {}));
    var Script$2;
    (function (Script) {
        (function (EventNames) {
            // keep-sorted start
            EventNames["Message"] = "script.message";
            EventNames["RealmCreated"] = "script.realmCreated";
            EventNames["RealmDestroyed"] = "script.realmDestroyed";
            // keep-sorted end
        })(Script.EventNames || (Script.EventNames = {}));
    })(Script$2 || (Script$2 = {}));
    var Log$1;
    (function (Log) {
        (function (EventNames) {
            EventNames["LogEntryAdded"] = "log.entryAdded";
        })(Log.EventNames || (Log.EventNames = {}));
    })(Log$1 || (Log$1 = {}));
    var BrowsingContext$2;
    (function (BrowsingContext) {
        (function (EventNames) {
            // keep-sorted start
            EventNames["ContextCreated"] = "browsingContext.contextCreated";
            EventNames["ContextDestroyed"] = "browsingContext.contextDestroyed";
            EventNames["DomContentLoaded"] = "browsingContext.domContentLoaded";
            EventNames["DownloadWillBegin"] = "browsingContext.downloadWillBegin";
            EventNames["FragmentNavigated"] = "browsingContext.fragmentNavigated";
            EventNames["Load"] = "browsingContext.load";
            EventNames["NavigationAborted"] = "browsingContext.navigationAborted";
            EventNames["NavigationFailed"] = "browsingContext.navigationFailed";
            EventNames["NavigationStarted"] = "browsingContext.navigationStarted";
            EventNames["UserPromptClosed"] = "browsingContext.userPromptClosed";
            EventNames["UserPromptOpened"] = "browsingContext.userPromptOpened";
            // keep-sorted end
        })(BrowsingContext.EventNames || (BrowsingContext.EventNames = {}));
    })(BrowsingContext$2 || (BrowsingContext$2 = {}));
    var Network$2;
    (function (Network) {
        (function (EventNames) {
            // keep-sorted start
            EventNames["AuthRequired"] = "network.authRequired";
            EventNames["BeforeRequestSent"] = "network.beforeRequestSent";
            EventNames["FetchError"] = "network.fetchError";
            EventNames["ResponseCompleted"] = "network.responseCompleted";
            EventNames["ResponseStarted"] = "network.responseStarted";
            // keep-sorted end
        })(Network.EventNames || (Network.EventNames = {}));
    })(Network$2 || (Network$2 = {}));
    var Bluetooth$2;
    (function (Bluetooth) {
        (function (EventNames) {
            EventNames["RequestDevicePromptUpdated"] = "bluetooth.requestDevicePromptUpdated";
        })(Bluetooth.EventNames || (Bluetooth.EventNames = {}));
    })(Bluetooth$2 || (Bluetooth$2 = {}));
    const EVENT_NAMES = new Set([
        // keep-sorted start
        ...Object.values(BiDiModule),
        ...Object.values(BrowsingContext$2.EventNames),
        ...Object.values(Log$1.EventNames),
        ...Object.values(Network$2.EventNames),
        ...Object.values(Script$2.EventNames),
        // keep-sorted end
    ]);

    class Exception extends Error {
        error;
        message;
        stacktrace;
        constructor(error, message, stacktrace) {
            super();
            this.error = error;
            this.message = message;
            this.stacktrace = stacktrace;
        }
        toErrorResponse(commandId) {
            return {
                type: 'error',
                id: commandId,
                error: this.error,
                message: this.message,
                stacktrace: this.stacktrace,
            };
        }
    }
    class InvalidArgumentException extends Exception {
        constructor(message, stacktrace) {
            super("invalid argument" /* ErrorCode.InvalidArgument */, message, stacktrace);
        }
    }
    class InvalidSelectorException extends Exception {
        constructor(message, stacktrace) {
            super("invalid selector" /* ErrorCode.InvalidSelector */, message, stacktrace);
        }
    }
    class MoveTargetOutOfBoundsException extends Exception {
        constructor(message, stacktrace) {
            super("move target out of bounds" /* ErrorCode.MoveTargetOutOfBounds */, message, stacktrace);
        }
    }
    class NoSuchAlertException extends Exception {
        constructor(message, stacktrace) {
            super("no such alert" /* ErrorCode.NoSuchAlert */, message, stacktrace);
        }
    }
    class NoSuchElementException extends Exception {
        constructor(message, stacktrace) {
            super("no such element" /* ErrorCode.NoSuchElement */, message, stacktrace);
        }
    }
    class NoSuchFrameException extends Exception {
        constructor(message, stacktrace) {
            super("no such frame" /* ErrorCode.NoSuchFrame */, message, stacktrace);
        }
    }
    class NoSuchHandleException extends Exception {
        constructor(message, stacktrace) {
            super("no such handle" /* ErrorCode.NoSuchHandle */, message, stacktrace);
        }
    }
    class NoSuchHistoryEntryException extends Exception {
        constructor(message, stacktrace) {
            super("no such history entry" /* ErrorCode.NoSuchHistoryEntry */, message, stacktrace);
        }
    }
    class NoSuchInterceptException extends Exception {
        constructor(message, stacktrace) {
            super("no such intercept" /* ErrorCode.NoSuchIntercept */, message, stacktrace);
        }
    }
    class NoSuchNodeException extends Exception {
        constructor(message, stacktrace) {
            super("no such node" /* ErrorCode.NoSuchNode */, message, stacktrace);
        }
    }
    class NoSuchRequestException extends Exception {
        constructor(message, stacktrace) {
            super("no such request" /* ErrorCode.NoSuchRequest */, message, stacktrace);
        }
    }
    class NoSuchScriptException extends Exception {
        constructor(message, stacktrace) {
            super("no such script" /* ErrorCode.NoSuchScript */, message, stacktrace);
        }
    }
    class NoSuchUserContextException extends Exception {
        constructor(message, stacktrace) {
            super("no such user context" /* ErrorCode.NoSuchUserContext */, message, stacktrace);
        }
    }
    class UnknownCommandException extends Exception {
        constructor(message, stacktrace) {
            super("unknown command" /* ErrorCode.UnknownCommand */, message, stacktrace);
        }
    }
    class UnknownErrorException extends Exception {
        constructor(message, stacktrace = new Error().stack) {
            super("unknown error" /* ErrorCode.UnknownError */, message, stacktrace);
        }
    }
    class UnableToCaptureScreenException extends Exception {
        constructor(message, stacktrace) {
            super("unable to capture screen" /* ErrorCode.UnableToCaptureScreen */, message, stacktrace);
        }
    }
    class UnsupportedOperationException extends Exception {
        constructor(message, stacktrace) {
            super("unsupported operation" /* ErrorCode.UnsupportedOperation */, message, stacktrace);
        }
    }
    class UnableToSetCookieException extends Exception {
        constructor(message, stacktrace) {
            super("unable to set cookie" /* ErrorCode.UnableToSetCookie */, message, stacktrace);
        }
    }
    class UnableToSetFileInputException extends Exception {
        constructor(message, stacktrace) {
            super("unable to set file input" /* ErrorCode.UnableToSetFileInput */, message, stacktrace);
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class BidiNoOpParser {
        // Bluetooth domain
        // keep-sorted start block=yes
        parseHandleRequestDevicePromptParams(params) {
            return params;
        }
        // keep-sorted end
        // Browser domain
        // keep-sorted start block=yes
        parseRemoveUserContextParams(params) {
            return params;
        }
        // keep-sorted end
        // Browsing Context domain
        // keep-sorted start block=yes
        parseActivateParams(params) {
            return params;
        }
        parseCaptureScreenshotParams(params) {
            return params;
        }
        parseCloseParams(params) {
            return params;
        }
        parseCreateParams(params) {
            return params;
        }
        parseGetTreeParams(params) {
            return params;
        }
        parseHandleUserPromptParams(params) {
            return params;
        }
        parseLocateNodesParams(params) {
            return params;
        }
        parseNavigateParams(params) {
            return params;
        }
        parsePrintParams(params) {
            return params;
        }
        parseReloadParams(params) {
            return params;
        }
        parseSetViewportParams(params) {
            return params;
        }
        parseTraverseHistoryParams(params) {
            return params;
        }
        // keep-sorted end
        // CDP domain
        // keep-sorted start block=yes
        parseGetSessionParams(params) {
            return params;
        }
        parseResolveRealmParams(params) {
            return params;
        }
        parseSendCommandParams(params) {
            return params;
        }
        // keep-sorted end
        // Script domain
        // keep-sorted start block=yes
        parseAddPreloadScriptParams(params) {
            return params;
        }
        parseCallFunctionParams(params) {
            return params;
        }
        parseDisownParams(params) {
            return params;
        }
        parseEvaluateParams(params) {
            return params;
        }
        parseGetRealmsParams(params) {
            return params;
        }
        parseRemovePreloadScriptParams(params) {
            return params;
        }
        // keep-sorted end
        // Input domain
        // keep-sorted start block=yes
        parsePerformActionsParams(params) {
            return params;
        }
        parseReleaseActionsParams(params) {
            return params;
        }
        parseSetFilesParams(params) {
            return params;
        }
        // keep-sorted end
        // Network domain
        // keep-sorted start block=yes
        parseAddInterceptParams(params) {
            return params;
        }
        parseContinueRequestParams(params) {
            return params;
        }
        parseContinueResponseParams(params) {
            return params;
        }
        parseContinueWithAuthParams(params) {
            return params;
        }
        parseFailRequestParams(params) {
            return params;
        }
        parseProvideResponseParams(params) {
            return params;
        }
        parseRemoveInterceptParams(params) {
            return params;
        }
        parseSetCacheBehavior(params) {
            return params;
        }
        // keep-sorted end
        // Permissions domain
        // keep-sorted start block=yes
        parseSetPermissionsParams(params) {
            return params;
        }
        // keep-sorted end
        // Session domain
        // keep-sorted start block=yes
        parseSubscribeParams(params) {
            return params;
        }
        // keep-sorted end
        // Storage domain
        // keep-sorted start block=yes
        parseDeleteCookiesParams(params) {
            return params;
        }
        parseGetCookiesParams(params) {
            return params;
        }
        parseSetCookieParams(params) {
            return params;
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class BrowserProcessor {
        #browserCdpClient;
        constructor(browserCdpClient) {
            this.#browserCdpClient = browserCdpClient;
        }
        close() {
            // Ensure that it is put at the end of the event loop.
            // This way we send back the response before closing the tab.
            setTimeout(() => this.#browserCdpClient.sendCommand('Browser.close'), 0);
            return {};
        }
        async createUserContext(params) {
            const request = {
                proxyServer: params['goog:proxyServer'] ?? undefined,
            };
            const proxyBypassList = params['goog:proxyBypassList'] ?? undefined;
            if (proxyBypassList) {
                request.proxyBypassList = proxyBypassList.join(',');
            }
            const context = await this.#browserCdpClient.sendCommand('Target.createBrowserContext', request);
            return {
                userContext: context.browserContextId,
            };
        }
        async removeUserContext(params) {
            const userContext = params.userContext;
            if (userContext === 'default') {
                throw new InvalidArgumentException('`default` user context cannot be removed');
            }
            try {
                await this.#browserCdpClient.sendCommand('Target.disposeBrowserContext', {
                    browserContextId: userContext,
                });
            }
            catch (err) {
                // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/target_handler.cc;l=1424;drc=c686e8f4fd379312469fe018f5c390e9c8f20d0d
                if (err.message.startsWith('Failed to find context with id')) {
                    throw new NoSuchUserContextException(err.message);
                }
                throw err;
            }
            return {};
        }
        async getUserContexts() {
            const result = await this.#browserCdpClient.sendCommand('Target.getBrowserContexts');
            return {
                userContexts: [
                    {
                        userContext: 'default',
                    },
                    ...result.browserContextIds.map((id) => {
                        return {
                            userContext: id,
                        };
                    }),
                ],
            };
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class CdpProcessor {
        #browsingContextStorage;
        #realmStorage;
        #cdpConnection;
        #browserCdpClient;
        constructor(browsingContextStorage, realmStorage, cdpConnection, browserCdpClient) {
            this.#browsingContextStorage = browsingContextStorage;
            this.#realmStorage = realmStorage;
            this.#cdpConnection = cdpConnection;
            this.#browserCdpClient = browserCdpClient;
        }
        getSession(params) {
            const context = params.context;
            const sessionId = this.#browsingContextStorage.getContext(context).cdpTarget.cdpSessionId;
            if (sessionId === undefined) {
                return {};
            }
            return { session: sessionId };
        }
        resolveRealm(params) {
            const context = params.realm;
            const realm = this.#realmStorage.getRealm({ realmId: context });
            if (realm === undefined) {
                throw new UnknownErrorException(`Could not find realm ${params.realm}`);
            }
            return { executionContextId: realm.executionContextId };
        }
        async sendCommand(params) {
            const client = params.session
                ? this.#cdpConnection.getCdpClient(params.session)
                : this.#browserCdpClient;
            const result = await client.sendCommand(params.method, params.params);
            return {
                result,
                session: params.session,
            };
        }
    }

    class BrowsingContextProcessor {
        #browserCdpClient;
        #browsingContextStorage;
        #eventManager;
        constructor(browserCdpClient, browsingContextStorage, eventManager) {
            this.#browserCdpClient = browserCdpClient;
            this.#browsingContextStorage = browsingContextStorage;
            this.#eventManager = eventManager;
            this.#eventManager.addSubscribeHook(BrowsingContext$2.EventNames.ContextCreated, this.#onContextCreatedSubscribeHook.bind(this));
        }
        getTree(params) {
            const resultContexts = params.root === undefined
                ? this.#browsingContextStorage.getTopLevelContexts()
                : [this.#browsingContextStorage.getContext(params.root)];
            return {
                contexts: resultContexts.map((c) => c.serializeToBidiValue(params.maxDepth ?? Number.MAX_VALUE)),
            };
        }
        async create(params) {
            let referenceContext;
            let userContext = 'default';
            if (params.referenceContext !== undefined) {
                referenceContext = this.#browsingContextStorage.getContext(params.referenceContext);
                if (!referenceContext.isTopLevelContext()) {
                    throw new InvalidArgumentException(`referenceContext should be a top-level context`);
                }
                userContext = referenceContext.userContext;
            }
            if (params.userContext !== undefined) {
                userContext = params.userContext;
            }
            const existingContexts = this.#browsingContextStorage
                .getAllContexts()
                .filter((context) => context.userContext === userContext);
            let newWindow = false;
            switch (params.type) {
                case "tab" /* BrowsingContext.CreateType.Tab */:
                    newWindow = false;
                    break;
                case "window" /* BrowsingContext.CreateType.Window */:
                    newWindow = true;
                    break;
            }
            if (!existingContexts.length) {
                // If there are no contexts in the given user context, we need to set
                // newWindow to true as newWindow=false will be rejected.
                newWindow = true;
            }
            let result;
            try {
                result = await this.#browserCdpClient.sendCommand('Target.createTarget', {
                    url: 'about:blank',
                    newWindow,
                    browserContextId: userContext === 'default' ? undefined : userContext,
                    background: params.background === true,
                });
            }
            catch (err) {
                if (
                // See https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/devtools/protocol/target_handler.cc;l=90;drc=e80392ac11e48a691f4309964cab83a3a59e01c8
                err.message.startsWith('Failed to find browser context with id') ||
                    // See https://source.chromium.org/chromium/chromium/src/+/main:headless/lib/browser/protocol/target_handler.cc;l=49;drc=e80392ac11e48a691f4309964cab83a3a59e01c8
                    err.message === 'browserContextId') {
                    throw new NoSuchUserContextException(`The context ${userContext} was not found`);
                }
                throw err;
            }
            // Wait for the new tab to be loaded to avoid race conditions in the
            // `browsingContext` events, when the `browsingContext.domContentLoaded` and
            // `browsingContext.load` events from the initial `about:blank` navigation
            // are emitted after the next navigation is started.
            // Details: https://github.com/web-platform-tests/wpt/issues/35846
            const contextId = result.targetId;
            const context = this.#browsingContextStorage.getContext(contextId);
            await context.lifecycleLoaded();
            return { context: context.id };
        }
        navigate(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            return context.navigate(params.url, params.wait ?? "none" /* BrowsingContext.ReadinessState.None */);
        }
        reload(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            return context.reload(params.ignoreCache ?? false, params.wait ?? "none" /* BrowsingContext.ReadinessState.None */);
        }
        async activate(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            if (!context.isTopLevelContext()) {
                throw new InvalidArgumentException('Activation is only supported on the top-level context');
            }
            await context.activate();
            return {};
        }
        async captureScreenshot(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            return await context.captureScreenshot(params);
        }
        async print(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            return await context.print(params);
        }
        async setViewport(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            if (!context.isTopLevelContext()) {
                throw new InvalidArgumentException('Emulating viewport is only supported on the top-level context');
            }
            await context.setViewport(params.viewport, params.devicePixelRatio);
            return {};
        }
        async traverseHistory(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            if (!context) {
                throw new InvalidArgumentException(`No browsing context with id ${params.context}`);
            }
            if (!context.isTopLevelContext()) {
                throw new InvalidArgumentException('Traversing history is only supported on the top-level context');
            }
            await context.traverseHistory(params.delta);
            return {};
        }
        async handleUserPrompt(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            try {
                await context.handleUserPrompt(params.accept, params.userText);
            }
            catch (error) {
                // Heuristically determine the error
                // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/page_handler.cc;l=1085?q=%22No%20dialog%20is%20showing%22&ss=chromium
                if (error.message?.includes('No dialog is showing')) {
                    throw new NoSuchAlertException('No dialog is showing');
                }
                throw error;
            }
            return {};
        }
        async close(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            if (!context.isTopLevelContext()) {
                throw new InvalidArgumentException(`Non top-level browsing context ${context.id} cannot be closed.`);
            }
            try {
                const detachedFromTargetPromise = new Promise((resolve) => {
                    const onContextDestroyed = (event) => {
                        if (event.targetId === params.context) {
                            this.#browserCdpClient.off('Target.detachedFromTarget', onContextDestroyed);
                            resolve();
                        }
                    };
                    this.#browserCdpClient.on('Target.detachedFromTarget', onContextDestroyed);
                });
                if (params.promptUnload) {
                    await context.close();
                }
                else {
                    await this.#browserCdpClient.sendCommand('Target.closeTarget', {
                        targetId: params.context,
                    });
                }
                // Sometimes CDP command finishes before `detachedFromTarget` event,
                // sometimes after. Wait for the CDP command to be finished, and then wait
                // for `detachedFromTarget` if it hasn't emitted.
                await detachedFromTargetPromise;
            }
            catch (error) {
                // Swallow error that arise from the page being destroyed
                // Example is navigating to faulty SSL certificate
                if (!(error.code === -32000 /* CdpErrorConstants.GENERIC_ERROR */ &&
                    error.message === 'Not attached to an active page')) {
                    throw error;
                }
            }
            return {};
        }
        async locateNodes(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            return await context.locateNodes(params);
        }
        #onContextCreatedSubscribeHook(contextId) {
            const context = this.#browsingContextStorage.getContext(contextId);
            const contextsToReport = [
                context,
                ...this.#browsingContextStorage.getContext(contextId).allChildren,
            ];
            contextsToReport.forEach((context) => {
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: BrowsingContext$2.EventNames.ContextCreated,
                    params: context.serializeToBidiValue(),
                }, context.id);
            });
            return Promise.resolve();
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    function assert(predicate, message) {
        if (!predicate) {
            throw new Error(message ?? 'Internal assertion failed.');
        }
    }

    /*
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Check if the given string is a single complex grapheme. A complex grapheme is one that
     * is made up of multiple characters.
     */
    function isSingleComplexGrapheme(value) {
        return isSingleGrapheme(value) && value.length > 1;
    }
    /**
     * Check if the given string is a single grapheme.
     */
    function isSingleGrapheme(value) {
        // Theoretically there can be some strings considered a grapheme in some locales, like
        // slovak "ch" digraph. Use english locale for consistency.
        // https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
        const segmenter = new Intl.Segmenter('en', { granularity: 'grapheme' });
        return [...segmenter.segment(value)].length === 1;
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class NoneSource {
        type = "none" /* SourceType.None */;
    }
    class KeySource {
        type = "key" /* SourceType.Key */;
        pressed = new Set();
        // This is a bitfield that matches the modifiers parameter of
        // https://chromedevtools.github.io/devtools-protocol/tot/Input/#method-dispatchKeyEvent
        #modifiers = 0;
        get modifiers() {
            return this.#modifiers;
        }
        get alt() {
            return (this.#modifiers & 1) === 1;
        }
        set alt(value) {
            this.#setModifier(value, 1);
        }
        get ctrl() {
            return (this.#modifiers & 2) === 2;
        }
        set ctrl(value) {
            this.#setModifier(value, 2);
        }
        get meta() {
            return (this.#modifiers & 4) === 4;
        }
        set meta(value) {
            this.#setModifier(value, 4);
        }
        get shift() {
            return (this.#modifiers & 8) === 8;
        }
        set shift(value) {
            this.#setModifier(value, 8);
        }
        #setModifier(value, bit) {
            if (value) {
                this.#modifiers |= bit;
            }
            else {
                this.#modifiers &= ~bit;
            }
        }
    }
    class PointerSource {
        type = "pointer" /* SourceType.Pointer */;
        subtype;
        pointerId;
        pressed = new Set();
        x = 0;
        y = 0;
        radiusX;
        radiusY;
        force;
        constructor(id, subtype) {
            this.pointerId = id;
            this.subtype = subtype;
        }
        // This is a bitfield that matches the buttons parameter of
        // https://chromedevtools.github.io/devtools-protocol/tot/Input/#method-dispatchMouseEvent
        get buttons() {
            let buttons = 0;
            for (const button of this.pressed) {
                switch (button) {
                    case 0:
                        buttons |= 1;
                        break;
                    case 1:
                        buttons |= 4;
                        break;
                    case 2:
                        buttons |= 2;
                        break;
                    case 3:
                        buttons |= 8;
                        break;
                    case 4:
                        buttons |= 16;
                        break;
                }
            }
            return buttons;
        }
        // --- Platform-specific code starts here ---
        // Input.dispatchMouseEvent doesn't know the concept of double click, so we
        // need to create the logic, similar to how it's done for OSes:
        // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:ui/events/event.cc;l=479
        static ClickContext = class ClickContext {
            static #DOUBLE_CLICK_TIME_MS = 500;
            static #MAX_DOUBLE_CLICK_RADIUS = 2;
            count = 0;
            #x;
            #y;
            #time;
            constructor(x, y, time) {
                this.#x = x;
                this.#y = y;
                this.#time = time;
            }
            compare(context) {
                return (
                // The click needs to be within a certain amount of ms.
                context.#time - this.#time > ClickContext.#DOUBLE_CLICK_TIME_MS ||
                    // The click needs to be within a certain square radius.
                    Math.abs(context.#x - this.#x) >
                        ClickContext.#MAX_DOUBLE_CLICK_RADIUS ||
                    Math.abs(context.#y - this.#y) > ClickContext.#MAX_DOUBLE_CLICK_RADIUS);
            }
        };
        #clickContexts = new Map();
        setClickCount(button, context) {
            let storedContext = this.#clickContexts.get(button);
            if (!storedContext || storedContext.compare(context)) {
                storedContext = context;
            }
            ++storedContext.count;
            this.#clickContexts.set(button, storedContext);
            return storedContext.count;
        }
        getClickCount(button) {
            return this.#clickContexts.get(button)?.count ?? 0;
        }
    }
    class WheelSource {
        type = "wheel" /* SourceType.Wheel */;
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Returns the normalized key value for a given key according to the table:
     * https://w3c.github.io/webdriver/#dfn-normalized-key-value
     */
    function getNormalizedKey(value) {
        switch (value) {
            case '\uE000':
                return 'Unidentified';
            case '\uE001':
                return 'Cancel';
            case '\uE002':
                return 'Help';
            case '\uE003':
                return 'Backspace';
            case '\uE004':
                return 'Tab';
            case '\uE005':
                return 'Clear';
            // Specification declares the '\uE006' to be `Return`, but it is not supported by
            // Chrome, so fall back to `Enter`, which aligns with WPT.
            case '\uE006':
            case '\uE007':
                return 'Enter';
            case '\uE008':
                return 'Shift';
            case '\uE009':
                return 'Control';
            case '\uE00A':
                return 'Alt';
            case '\uE00B':
                return 'Pause';
            case '\uE00C':
                return 'Escape';
            case '\uE00D':
                return ' ';
            case '\uE00E':
                return 'PageUp';
            case '\uE00F':
                return 'PageDown';
            case '\uE010':
                return 'End';
            case '\uE011':
                return 'Home';
            case '\uE012':
                return 'ArrowLeft';
            case '\uE013':
                return 'ArrowUp';
            case '\uE014':
                return 'ArrowRight';
            case '\uE015':
                return 'ArrowDown';
            case '\uE016':
                return 'Insert';
            case '\uE017':
                return 'Delete';
            case '\uE018':
                return ';';
            case '\uE019':
                return '=';
            case '\uE01A':
                return '0';
            case '\uE01B':
                return '1';
            case '\uE01C':
                return '2';
            case '\uE01D':
                return '3';
            case '\uE01E':
                return '4';
            case '\uE01F':
                return '5';
            case '\uE020':
                return '6';
            case '\uE021':
                return '7';
            case '\uE022':
                return '8';
            case '\uE023':
                return '9';
            case '\uE024':
                return '*';
            case '\uE025':
                return '+';
            case '\uE026':
                return ',';
            case '\uE027':
                return '-';
            case '\uE028':
                return '.';
            case '\uE029':
                return '/';
            case '\uE031':
                return 'F1';
            case '\uE032':
                return 'F2';
            case '\uE033':
                return 'F3';
            case '\uE034':
                return 'F4';
            case '\uE035':
                return 'F5';
            case '\uE036':
                return 'F6';
            case '\uE037':
                return 'F7';
            case '\uE038':
                return 'F8';
            case '\uE039':
                return 'F9';
            case '\uE03A':
                return 'F10';
            case '\uE03B':
                return 'F11';
            case '\uE03C':
                return 'F12';
            case '\uE03D':
                return 'Meta';
            case '\uE040':
                return 'ZenkakuHankaku';
            case '\uE050':
                return 'Shift';
            case '\uE051':
                return 'Control';
            case '\uE052':
                return 'Alt';
            case '\uE053':
                return 'Meta';
            case '\uE054':
                return 'PageUp';
            case '\uE055':
                return 'PageDown';
            case '\uE056':
                return 'End';
            case '\uE057':
                return 'Home';
            case '\uE058':
                return 'ArrowLeft';
            case '\uE059':
                return 'ArrowUp';
            case '\uE05A':
                return 'ArrowRight';
            case '\uE05B':
                return 'ArrowDown';
            case '\uE05C':
                return 'Insert';
            case '\uE05D':
                return 'Delete';
            default:
                return value;
        }
    }
    /**
     * Returns the key code for a given key according to the table:
     * https://w3c.github.io/webdriver/#dfn-shifted-character
     */
    function getKeyCode(key) {
        switch (key) {
            case '`':
            case '~':
                return 'Backquote';
            case '\\':
            case '|':
                return 'Backslash';
            case '\uE003':
                return 'Backspace';
            case '[':
            case '{':
                return 'BracketLeft';
            case ']':
            case '}':
                return 'BracketRight';
            case ',':
            case '<':
                return 'Comma';
            case '0':
            case ')':
                return 'Digit0';
            case '1':
            case '!':
                return 'Digit1';
            case '2':
            case '@':
                return 'Digit2';
            case '3':
            case '#':
                return 'Digit3';
            case '4':
            case '$':
                return 'Digit4';
            case '5':
            case '%':
                return 'Digit5';
            case '6':
            case '^':
                return 'Digit6';
            case '7':
            case '&':
                return 'Digit7';
            case '8':
            case '*':
                return 'Digit8';
            case '9':
            case '(':
                return 'Digit9';
            case '=':
            case '+':
                return 'Equal';
            // The spec declares the '<' to be `IntlBackslash` as well, but it is already covered
            // in the `Comma` above.
            case '>':
                return 'IntlBackslash';
            case 'a':
            case 'A':
                return 'KeyA';
            case 'b':
            case 'B':
                return 'KeyB';
            case 'c':
            case 'C':
                return 'KeyC';
            case 'd':
            case 'D':
                return 'KeyD';
            case 'e':
            case 'E':
                return 'KeyE';
            case 'f':
            case 'F':
                return 'KeyF';
            case 'g':
            case 'G':
                return 'KeyG';
            case 'h':
            case 'H':
                return 'KeyH';
            case 'i':
            case 'I':
                return 'KeyI';
            case 'j':
            case 'J':
                return 'KeyJ';
            case 'k':
            case 'K':
                return 'KeyK';
            case 'l':
            case 'L':
                return 'KeyL';
            case 'm':
            case 'M':
                return 'KeyM';
            case 'n':
            case 'N':
                return 'KeyN';
            case 'o':
            case 'O':
                return 'KeyO';
            case 'p':
            case 'P':
                return 'KeyP';
            case 'q':
            case 'Q':
                return 'KeyQ';
            case 'r':
            case 'R':
                return 'KeyR';
            case 's':
            case 'S':
                return 'KeyS';
            case 't':
            case 'T':
                return 'KeyT';
            case 'u':
            case 'U':
                return 'KeyU';
            case 'v':
            case 'V':
                return 'KeyV';
            case 'w':
            case 'W':
                return 'KeyW';
            case 'x':
            case 'X':
                return 'KeyX';
            case 'y':
            case 'Y':
                return 'KeyY';
            case 'z':
            case 'Z':
                return 'KeyZ';
            case '-':
            case '_':
                return 'Minus';
            case '.':
                return 'Period';
            case "'":
            case '"':
                return 'Quote';
            case ';':
            case ':':
                return 'Semicolon';
            case '/':
            case '?':
                return 'Slash';
            case '\uE00A':
                return 'AltLeft';
            case '\uE052':
                return 'AltRight';
            case '\uE009':
                return 'ControlLeft';
            case '\uE051':
                return 'ControlRight';
            case '\uE006':
                return 'Enter';
            case '\uE00B':
                return 'Pause';
            case '\uE03D':
                return 'MetaLeft';
            case '\uE053':
                return 'MetaRight';
            case '\uE008':
                return 'ShiftLeft';
            case '\uE050':
                return 'ShiftRight';
            case ' ':
            case '\uE00D':
                return 'Space';
            case '\uE004':
                return 'Tab';
            case '\uE017':
                return 'Delete';
            case '\uE010':
                return 'End';
            case '\uE002':
                return 'Help';
            case '\uE011':
                return 'Home';
            case '\uE016':
                return 'Insert';
            case '\uE00F':
                return 'PageDown';
            case '\uE00E':
                return 'PageUp';
            case '\uE015':
                return 'ArrowDown';
            case '\uE012':
                return 'ArrowLeft';
            case '\uE014':
                return 'ArrowRight';
            case '\uE013':
                return 'ArrowUp';
            case '\uE00C':
                return 'Escape';
            case '\uE031':
                return 'F1';
            case '\uE032':
                return 'F2';
            case '\uE033':
                return 'F3';
            case '\uE034':
                return 'F4';
            case '\uE035':
                return 'F5';
            case '\uE036':
                return 'F6';
            case '\uE037':
                return 'F7';
            case '\uE038':
                return 'F8';
            case '\uE039':
                return 'F9';
            case '\uE03A':
                return 'F10';
            case '\uE03B':
                return 'F11';
            case '\uE03C':
                return 'F12';
            case '\uE019':
                return 'NumpadEqual';
            case '\uE01A':
            case '\uE05C':
                return 'Numpad0';
            case '\uE01B':
            case '\uE056':
                return 'Numpad1';
            case '\uE01C':
            case '\uE05B':
                return 'Numpad2';
            case '\uE01D':
            case '\uE055':
                return 'Numpad3';
            case '\uE01E':
            case '\uE058':
                return 'Numpad4';
            case '\uE01F':
                return 'Numpad5';
            case '\uE020':
            case '\uE05A':
                return 'Numpad6';
            case '\uE021':
            case '\uE057':
                return 'Numpad7';
            case '\uE022':
            case '\uE059':
                return 'Numpad8';
            case '\uE023':
            case '\uE054':
                return 'Numpad9';
            case '\uE025':
                return 'NumpadAdd';
            case '\uE026':
                return 'NumpadComma';
            case '\uE028':
            case '\uE05D':
                return 'NumpadDecimal';
            case '\uE029':
                return 'NumpadDivide';
            case '\uE007':
                return 'NumpadEnter';
            case '\uE024':
                return 'NumpadMultiply';
            case '\uE027':
                return 'NumpadSubtract';
            default:
                return;
        }
    }
    /**
     * Returns the location of the key according to the table:
     * https://w3c.github.io/webdriver/#dfn-key-location
     */
    function getKeyLocation(key) {
        switch (key) {
            case '\uE007':
            case '\uE008':
            case '\uE009':
            case '\uE00A':
            case '\uE03D':
                return 1;
            case '\uE019':
            case '\uE01A':
            case '\uE01B':
            case '\uE01C':
            case '\uE01D':
            case '\uE01E':
            case '\uE01F':
            case '\uE020':
            case '\uE021':
            case '\uE022':
            case '\uE023':
            case '\uE024':
            case '\uE025':
            case '\uE026':
            case '\uE027':
            case '\uE028':
            case '\uE029':
            case '\uE054':
            case '\uE055':
            case '\uE056':
            case '\uE057':
            case '\uE058':
            case '\uE059':
            case '\uE05A':
            case '\uE05B':
            case '\uE05C':
            case '\uE05D':
                return 3;
            case '\uE050':
            case '\uE051':
            case '\uE052':
            case '\uE053':
                return 2;
            default:
                return 0;
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    // TODO: Remove this once https://crrev.com/c/4548290 is stably in Chromium.
    // `Input.dispatchKeyboardEvent` will automatically handle these conversions.
    const KeyToKeyCode = {
        '0': 48,
        '1': 49,
        '2': 50,
        '3': 51,
        '4': 52,
        '5': 53,
        '6': 54,
        '7': 55,
        '8': 56,
        '9': 57,
        Abort: 3,
        Help: 6,
        Backspace: 8,
        Tab: 9,
        Numpad5: 12,
        NumpadEnter: 13,
        Enter: 13,
        '\\r': 13,
        '\\n': 13,
        ShiftLeft: 16,
        ShiftRight: 16,
        ControlLeft: 17,
        ControlRight: 17,
        AltLeft: 18,
        AltRight: 18,
        Pause: 19,
        CapsLock: 20,
        Escape: 27,
        Convert: 28,
        NonConvert: 29,
        Space: 32,
        Numpad9: 33,
        PageUp: 33,
        Numpad3: 34,
        PageDown: 34,
        End: 35,
        Numpad1: 35,
        Home: 36,
        Numpad7: 36,
        ArrowLeft: 37,
        Numpad4: 37,
        Numpad8: 38,
        ArrowUp: 38,
        ArrowRight: 39,
        Numpad6: 39,
        Numpad2: 40,
        ArrowDown: 40,
        Select: 41,
        Open: 43,
        PrintScreen: 44,
        Insert: 45,
        Numpad0: 45,
        Delete: 46,
        NumpadDecimal: 46,
        Digit0: 48,
        Digit1: 49,
        Digit2: 50,
        Digit3: 51,
        Digit4: 52,
        Digit5: 53,
        Digit6: 54,
        Digit7: 55,
        Digit8: 56,
        Digit9: 57,
        KeyA: 65,
        KeyB: 66,
        KeyC: 67,
        KeyD: 68,
        KeyE: 69,
        KeyF: 70,
        KeyG: 71,
        KeyH: 72,
        KeyI: 73,
        KeyJ: 74,
        KeyK: 75,
        KeyL: 76,
        KeyM: 77,
        KeyN: 78,
        KeyO: 79,
        KeyP: 80,
        KeyQ: 81,
        KeyR: 82,
        KeyS: 83,
        KeyT: 84,
        KeyU: 85,
        KeyV: 86,
        KeyW: 87,
        KeyX: 88,
        KeyY: 89,
        KeyZ: 90,
        MetaLeft: 91,
        MetaRight: 92,
        ContextMenu: 93,
        NumpadMultiply: 106,
        NumpadAdd: 107,
        NumpadSubtract: 109,
        NumpadDivide: 111,
        F1: 112,
        F2: 113,
        F3: 114,
        F4: 115,
        F5: 116,
        F6: 117,
        F7: 118,
        F8: 119,
        F9: 120,
        F10: 121,
        F11: 122,
        F12: 123,
        F13: 124,
        F14: 125,
        F15: 126,
        F16: 127,
        F17: 128,
        F18: 129,
        F19: 130,
        F20: 131,
        F21: 132,
        F22: 133,
        F23: 134,
        F24: 135,
        NumLock: 144,
        ScrollLock: 145,
        AudioVolumeMute: 173,
        AudioVolumeDown: 174,
        AudioVolumeUp: 175,
        MediaTrackNext: 176,
        MediaTrackPrevious: 177,
        MediaStop: 178,
        MediaPlayPause: 179,
        Semicolon: 186,
        Equal: 187,
        NumpadEqual: 187,
        Comma: 188,
        Minus: 189,
        Period: 190,
        Slash: 191,
        Backquote: 192,
        BracketLeft: 219,
        Backslash: 220,
        BracketRight: 221,
        Quote: 222,
        AltGraph: 225,
        Props: 247,
        Cancel: 3,
        Clear: 12,
        Shift: 16,
        Control: 17,
        Alt: 18,
        Accept: 30,
        ModeChange: 31,
        ' ': 32,
        Print: 42,
        Execute: 43,
        '\\u0000': 46,
        a: 65,
        b: 66,
        c: 67,
        d: 68,
        e: 69,
        f: 70,
        g: 71,
        h: 72,
        i: 73,
        j: 74,
        k: 75,
        l: 76,
        m: 77,
        n: 78,
        o: 79,
        p: 80,
        q: 81,
        r: 82,
        s: 83,
        t: 84,
        u: 85,
        v: 86,
        w: 87,
        x: 88,
        y: 89,
        z: 90,
        Meta: 91,
        '*': 106,
        '+': 107,
        '-': 109,
        '/': 111,
        ';': 186,
        '=': 187,
        ',': 188,
        '.': 190,
        '`': 192,
        '[': 219,
        '\\\\': 220,
        ']': 221,
        "'": 222,
        Attn: 246,
        CrSel: 247,
        ExSel: 248,
        EraseEof: 249,
        Play: 250,
        ZoomOut: 251,
        ')': 48,
        '!': 49,
        '@': 50,
        '#': 51,
        $: 52,
        '%': 53,
        '^': 54,
        '&': 55,
        '(': 57,
        A: 65,
        B: 66,
        C: 67,
        D: 68,
        E: 69,
        F: 70,
        G: 71,
        H: 72,
        I: 73,
        J: 74,
        K: 75,
        L: 76,
        M: 77,
        N: 78,
        O: 79,
        P: 80,
        Q: 81,
        R: 82,
        S: 83,
        T: 84,
        U: 85,
        V: 86,
        W: 87,
        X: 88,
        Y: 89,
        Z: 90,
        ':': 186,
        '<': 188,
        _: 189,
        '>': 190,
        '?': 191,
        '~': 192,
        '{': 219,
        '|': 220,
        '}': 221,
        '"': 222,
        Camera: 44,
        EndCall: 95,
        VolumeDown: 182,
        VolumeUp: 183,
    };

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** https://w3c.github.io/webdriver/#dfn-center-point */
    const CALCULATE_IN_VIEW_CENTER_PT_DECL = ((i) => {
        const t = i.getClientRects()[0], e = Math.max(0, Math.min(t.x, t.x + t.width)), n = Math.min(window.innerWidth, Math.max(t.x, t.x + t.width)), h = Math.max(0, Math.min(t.y, t.y + t.height)), m = Math.min(window.innerHeight, Math.max(t.y, t.y + t.height));
        return [e + ((n - e) >> 1), h + ((m - h) >> 1)];
    }).toString();
    const IS_MAC_DECL = (() => {
        return navigator.platform.toLowerCase().includes('mac');
    }).toString();
    async function getElementCenter(context, element) {
        const sandbox = await context.getOrCreateSandbox(undefined);
        const result = await sandbox.callFunction(CALCULATE_IN_VIEW_CENTER_PT_DECL, false, { type: 'undefined' }, [element]);
        if (result.type === 'exception') {
            throw new NoSuchElementException(`Origin element ${element.sharedId} was not found`);
        }
        assert(result.result.type === 'array');
        assert(result.result.value?.[0]?.type === 'number');
        assert(result.result.value?.[1]?.type === 'number');
        const { result: { value: [{ value: x }, { value: y }], }, } = result;
        return { x: x, y: y };
    }
    class ActionDispatcher {
        static isMacOS = async (context) => {
            const result = await (await context.getOrCreateSandbox(undefined)).callFunction(IS_MAC_DECL, false);
            assert(result.type !== 'exception');
            assert(result.result.type === 'boolean');
            return result.result.value;
        };
        #tickStart = 0;
        #tickDuration = 0;
        #inputState;
        #context;
        #isMacOS;
        constructor(inputState, context, isMacOS) {
            this.#inputState = inputState;
            this.#context = context;
            this.#isMacOS = isMacOS;
        }
        async dispatchActions(optionsByTick) {
            await this.#inputState.queue.run(async () => {
                for (const options of optionsByTick) {
                    await this.dispatchTickActions(options);
                }
            });
        }
        async dispatchTickActions(options) {
            this.#tickStart = performance.now();
            this.#tickDuration = 0;
            for (const { action } of options) {
                if ('duration' in action && action.duration !== undefined) {
                    this.#tickDuration = Math.max(this.#tickDuration, action.duration);
                }
            }
            const promises = [
                new Promise((resolve) => setTimeout(resolve, this.#tickDuration)),
            ];
            for (const option of options) {
                // In theory we have to wait for each action to happen, but CDP is serial,
                // so as an optimization, we queue all CDP commands at once and await all
                // of them.
                promises.push(this.#dispatchAction(option));
            }
            await Promise.all(promises);
        }
        async #dispatchAction({ id, action }) {
            const source = this.#inputState.get(id);
            const keyState = this.#inputState.getGlobalKeyState();
            switch (action.type) {
                case 'keyDown': {
                    // SAFETY: The source is validated before.
                    await this.#dispatchKeyDownAction(source, action);
                    this.#inputState.cancelList.push({
                        id,
                        action: {
                            ...action,
                            type: 'keyUp',
                        },
                    });
                    break;
                }
                case 'keyUp': {
                    // SAFETY: The source is validated before.
                    await this.#dispatchKeyUpAction(source, action);
                    break;
                }
                case 'pause': {
                    // TODO: Implement waiting on the input source.
                    break;
                }
                case 'pointerDown': {
                    // SAFETY: The source is validated before.
                    await this.#dispatchPointerDownAction(source, keyState, action);
                    this.#inputState.cancelList.push({
                        id,
                        action: {
                            ...action,
                            type: 'pointerUp',
                        },
                    });
                    break;
                }
                case 'pointerMove': {
                    // SAFETY: The source is validated before.
                    await this.#dispatchPointerMoveAction(source, keyState, action);
                    break;
                }
                case 'pointerUp': {
                    // SAFETY: The source is validated before.
                    await this.#dispatchPointerUpAction(source, keyState, action);
                    break;
                }
                case 'scroll': {
                    // SAFETY: The source is validated before.
                    await this.#dispatchScrollAction(source, keyState, action);
                    break;
                }
            }
        }
        async #dispatchPointerDownAction(source, keyState, action) {
            const { button } = action;
            if (source.pressed.has(button)) {
                return;
            }
            source.pressed.add(button);
            const { x, y, subtype: pointerType } = source;
            const { width, height, pressure, twist, tangentialPressure } = action;
            const { tiltX, tiltY } = getTilt(action);
            // --- Platform-specific code begins here ---
            const { modifiers } = keyState;
            const { radiusX, radiusY } = getRadii(width ?? 1, height ?? 1);
            switch (pointerType) {
                case "mouse" /* Input.PointerType.Mouse */:
                case "pen" /* Input.PointerType.Pen */:
                    // TODO: Implement width and height when available.
                    await this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchMouseEvent', {
                        type: 'mousePressed',
                        x,
                        y,
                        modifiers,
                        button: getCdpButton(button),
                        buttons: source.buttons,
                        clickCount: source.setClickCount(button, new PointerSource.ClickContext(x, y, performance.now())),
                        pointerType,
                        tangentialPressure,
                        tiltX,
                        tiltY,
                        twist,
                        force: pressure,
                    });
                    break;
                case "touch" /* Input.PointerType.Touch */:
                    await this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchTouchEvent', {
                        type: 'touchStart',
                        touchPoints: [
                            {
                                x,
                                y,
                                radiusX,
                                radiusY,
                                tangentialPressure,
                                tiltX,
                                tiltY,
                                twist,
                                force: pressure,
                                id: source.pointerId,
                            },
                        ],
                        modifiers,
                    });
                    break;
            }
            source.radiusX = radiusX;
            source.radiusY = radiusY;
            source.force = pressure;
            // --- Platform-specific code ends here ---
        }
        #dispatchPointerUpAction(source, keyState, action) {
            const { button } = action;
            if (!source.pressed.has(button)) {
                return;
            }
            source.pressed.delete(button);
            const { x, y, force, radiusX, radiusY, subtype: pointerType } = source;
            // --- Platform-specific code begins here ---
            const { modifiers } = keyState;
            switch (pointerType) {
                case "mouse" /* Input.PointerType.Mouse */:
                case "pen" /* Input.PointerType.Pen */:
                    // TODO: Implement width and height when available.
                    return this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchMouseEvent', {
                        type: 'mouseReleased',
                        x,
                        y,
                        modifiers,
                        button: getCdpButton(button),
                        buttons: source.buttons,
                        clickCount: source.getClickCount(button),
                        pointerType,
                    });
                case "touch" /* Input.PointerType.Touch */:
                    return this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchTouchEvent', {
                        type: 'touchEnd',
                        touchPoints: [
                            {
                                x,
                                y,
                                id: source.pointerId,
                                force,
                                radiusX,
                                radiusY,
                            },
                        ],
                        modifiers,
                    });
            }
            // --- Platform-specific code ends here ---
        }
        async #dispatchPointerMoveAction(source, keyState, action) {
            const { x: startX, y: startY, subtype: pointerType } = source;
            const { width, height, pressure, twist, tangentialPressure, x: offsetX, y: offsetY, origin = 'viewport', duration = this.#tickDuration, } = action;
            const { tiltX, tiltY } = getTilt(action);
            const { radiusX, radiusY } = getRadii(width ?? 1, height ?? 1);
            const { targetX, targetY } = await this.#getCoordinateFromOrigin(origin, offsetX, offsetY, startX, startY);
            if (targetX < 0 || targetY < 0) {
                throw new MoveTargetOutOfBoundsException(`Cannot move beyond viewport (x: ${targetX}, y: ${targetY})`);
            }
            let last;
            do {
                const ratio = duration > 0 ? (performance.now() - this.#tickStart) / duration : 1;
                last = ratio >= 1;
                let x;
                let y;
                if (last) {
                    x = targetX;
                    y = targetY;
                }
                else {
                    x = Math.round(ratio * (targetX - startX) + startX);
                    y = Math.round(ratio * (targetY - startY) + startY);
                }
                if (source.x !== x || source.y !== y) {
                    // --- Platform-specific code begins here ---
                    const { modifiers } = keyState;
                    switch (pointerType) {
                        case "mouse" /* Input.PointerType.Mouse */:
                            // TODO: Implement width and height when available.
                            await this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchMouseEvent', {
                                type: 'mouseMoved',
                                x,
                                y,
                                modifiers,
                                clickCount: 0,
                                button: getCdpButton(source.pressed.values().next().value ?? 5),
                                buttons: source.buttons,
                                pointerType,
                                tangentialPressure,
                                tiltX,
                                tiltY,
                                twist,
                                force: pressure,
                            });
                            break;
                        case "pen" /* Input.PointerType.Pen */:
                            if (source.pressed.size !== 0) {
                                // Empty `source.pressed.size` means the pen is not detected by digitizer.
                                // Dispatch a mouse event for the pen only if either:
                                // 1. the pen is hovering over the digitizer (0);
                                // 2. the pen is in contact with the digitizer (1);
                                // 3. the pen has at least one button pressed (2, 4, etc).
                                // https://www.w3.org/TR/pointerevents/#the-buttons-property
                                // TODO: Implement width and height when available.
                                await this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchMouseEvent', {
                                    type: 'mouseMoved',
                                    x,
                                    y,
                                    modifiers,
                                    clickCount: 0,
                                    button: getCdpButton(source.pressed.values().next().value ?? 5),
                                    buttons: source.buttons,
                                    pointerType,
                                    tangentialPressure,
                                    tiltX,
                                    tiltY,
                                    twist,
                                    force: pressure ?? 0.5,
                                });
                            }
                            break;
                        case "touch" /* Input.PointerType.Touch */:
                            if (source.pressed.size !== 0) {
                                await this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchTouchEvent', {
                                    type: 'touchMove',
                                    touchPoints: [
                                        {
                                            x,
                                            y,
                                            radiusX,
                                            radiusY,
                                            tangentialPressure,
                                            tiltX,
                                            tiltY,
                                            twist,
                                            force: pressure,
                                            id: source.pointerId,
                                        },
                                    ],
                                    modifiers,
                                });
                            }
                            break;
                    }
                    // --- Platform-specific code ends here ---
                    source.x = x;
                    source.y = y;
                    source.radiusX = radiusX;
                    source.radiusY = radiusY;
                    source.force = pressure;
                }
            } while (!last);
        }
        async #getCoordinateFromOrigin(origin, offsetX, offsetY, startX, startY) {
            let targetX;
            let targetY;
            switch (origin) {
                case 'viewport':
                    targetX = offsetX;
                    targetY = offsetY;
                    break;
                case 'pointer':
                    targetX = startX + offsetX;
                    targetY = startY + offsetY;
                    break;
                default: {
                    const { x: posX, y: posY } = await getElementCenter(this.#context, origin.element);
                    // SAFETY: These can never be special numbers.
                    targetX = posX + offsetX;
                    targetY = posY + offsetY;
                    break;
                }
            }
            return { targetX, targetY };
        }
        async #dispatchScrollAction(_source, keyState, action) {
            const { deltaX: targetDeltaX, deltaY: targetDeltaY, x: offsetX, y: offsetY, origin = 'viewport', duration = this.#tickDuration, } = action;
            if (origin === 'pointer') {
                throw new InvalidArgumentException('"pointer" origin is invalid for scrolling.');
            }
            const { targetX, targetY } = await this.#getCoordinateFromOrigin(origin, offsetX, offsetY, 0, 0);
            if (targetX < 0 || targetY < 0) {
                throw new MoveTargetOutOfBoundsException(`Cannot move beyond viewport (x: ${targetX}, y: ${targetY})`);
            }
            let currentDeltaX = 0;
            let currentDeltaY = 0;
            let last;
            do {
                const ratio = duration > 0 ? (performance.now() - this.#tickStart) / duration : 1;
                last = ratio >= 1;
                let deltaX;
                let deltaY;
                if (last) {
                    deltaX = targetDeltaX - currentDeltaX;
                    deltaY = targetDeltaY - currentDeltaY;
                }
                else {
                    deltaX = Math.round(ratio * targetDeltaX - currentDeltaX);
                    deltaY = Math.round(ratio * targetDeltaY - currentDeltaY);
                }
                if (deltaX !== 0 || deltaY !== 0) {
                    // --- Platform-specific code begins here ---
                    const { modifiers } = keyState;
                    await this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchMouseEvent', {
                        type: 'mouseWheel',
                        deltaX,
                        deltaY,
                        x: targetX,
                        y: targetY,
                        modifiers,
                    });
                    // --- Platform-specific code ends here ---
                    currentDeltaX += deltaX;
                    currentDeltaY += deltaY;
                }
            } while (!last);
        }
        async #dispatchKeyDownAction(source, action) {
            const rawKey = action.value;
            if (!isSingleGrapheme(rawKey)) {
                // https://w3c.github.io/webdriver/#dfn-process-a-key-action
                // WebDriver spec allows a grapheme to be used.
                throw new InvalidArgumentException(`Invalid key value: ${rawKey}`);
            }
            const isGrapheme = isSingleComplexGrapheme(rawKey);
            const key = getNormalizedKey(rawKey);
            const repeat = source.pressed.has(key);
            const code = getKeyCode(rawKey);
            const location = getKeyLocation(rawKey);
            switch (key) {
                case 'Alt':
                    source.alt = true;
                    break;
                case 'Shift':
                    source.shift = true;
                    break;
                case 'Control':
                    source.ctrl = true;
                    break;
                case 'Meta':
                    source.meta = true;
                    break;
            }
            source.pressed.add(key);
            const { modifiers } = source;
            // --- Platform-specific code begins here ---
            // The spread is a little hack so JS gives us an array of unicode characters
            // to measure.
            const unmodifiedText = getKeyEventUnmodifiedText(key, source, isGrapheme);
            const text = getKeyEventText(code ?? '', source) ?? unmodifiedText;
            let command;
            // The following commands need to be declared because Chromium doesn't
            // handle them. See
            // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/blink/renderer/core/editing/editing_behavior.cc;l=169;drc=b8143cf1dfd24842890fcd831c4f5d909bef4fc4;bpv=0;bpt=1.
            if (this.#isMacOS && source.meta) {
                switch (code) {
                    case 'KeyA':
                        command = 'SelectAll';
                        break;
                    case 'KeyC':
                        command = 'Copy';
                        break;
                    case 'KeyV':
                        command = source.shift ? 'PasteAndMatchStyle' : 'Paste';
                        break;
                    case 'KeyX':
                        command = 'Cut';
                        break;
                    case 'KeyZ':
                        command = source.shift ? 'Redo' : 'Undo';
                        break;
                    // Intentionally empty.
                }
            }
            const promises = [
                this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchKeyEvent', {
                    type: text ? 'keyDown' : 'rawKeyDown',
                    windowsVirtualKeyCode: KeyToKeyCode[key],
                    key,
                    code,
                    text,
                    unmodifiedText,
                    autoRepeat: repeat,
                    isSystemKey: source.alt || undefined,
                    location: location < 3 ? location : undefined,
                    isKeypad: location === 3,
                    modifiers,
                    commands: command ? [command] : undefined,
                }),
            ];
            // Drag cancelling happens on escape.
            if (key === 'Escape') {
                if (!source.alt &&
                    ((this.#isMacOS && !source.ctrl && !source.meta) || !this.#isMacOS)) {
                    promises.push(this.#context.cdpTarget.cdpClient.sendCommand('Input.cancelDragging'));
                }
            }
            await Promise.all(promises);
            // --- Platform-specific code ends here ---
        }
        #dispatchKeyUpAction(source, action) {
            const rawKey = action.value;
            if (!isSingleGrapheme(rawKey)) {
                // https://w3c.github.io/webdriver/#dfn-process-a-key-action
                // WebDriver spec allows a grapheme to be used.
                throw new InvalidArgumentException(`Invalid key value: ${rawKey}`);
            }
            const isGrapheme = isSingleComplexGrapheme(rawKey);
            const key = getNormalizedKey(rawKey);
            if (!source.pressed.has(key)) {
                return;
            }
            const code = getKeyCode(rawKey);
            const location = getKeyLocation(rawKey);
            switch (key) {
                case 'Alt':
                    source.alt = false;
                    break;
                case 'Shift':
                    source.shift = false;
                    break;
                case 'Control':
                    source.ctrl = false;
                    break;
                case 'Meta':
                    source.meta = false;
                    break;
            }
            source.pressed.delete(key);
            const { modifiers } = source;
            // --- Platform-specific code begins here ---
            // The spread is a little hack so JS gives us an array of unicode characters
            // to measure.
            const unmodifiedText = getKeyEventUnmodifiedText(key, source, isGrapheme);
            const text = getKeyEventText(code ?? '', source) ?? unmodifiedText;
            return this.#context.cdpTarget.cdpClient.sendCommand('Input.dispatchKeyEvent', {
                type: 'keyUp',
                windowsVirtualKeyCode: KeyToKeyCode[key],
                key,
                code,
                text,
                unmodifiedText,
                location: location < 3 ? location : undefined,
                isSystemKey: source.alt || undefined,
                isKeypad: location === 3,
                modifiers,
            });
            // --- Platform-specific code ends here ---
        }
    }
    /**
     * Translates a non-grapheme key to either an `undefined` for a special keys, or a single
     * character modified by shift if needed.
     */
    const getKeyEventUnmodifiedText = (key, source, isGrapheme) => {
        if (isGrapheme) {
            // Graphemes should be presented as text in the CDP command.
            return key;
        }
        if (key === 'Enter') {
            return '\r';
        }
        // If key is not a single character, it is a normalized key value, and should be
        // presented as key, not text in the CDP command.
        return [...key].length === 1
            ? source.shift
                ? key.toLocaleUpperCase('en-US')
                : key
            : undefined;
    };
    const getKeyEventText = (code, source) => {
        if (source.ctrl) {
            switch (code) {
                case 'Digit2':
                    if (source.shift) {
                        return '\x00';
                    }
                    break;
                case 'KeyA':
                    return '\x01';
                case 'KeyB':
                    return '\x02';
                case 'KeyC':
                    return '\x03';
                case 'KeyD':
                    return '\x04';
                case 'KeyE':
                    return '\x05';
                case 'KeyF':
                    return '\x06';
                case 'KeyG':
                    return '\x07';
                case 'KeyH':
                    return '\x08';
                case 'KeyI':
                    return '\x09';
                case 'KeyJ':
                    return '\x0A';
                case 'KeyK':
                    return '\x0B';
                case 'KeyL':
                    return '\x0C';
                case 'KeyM':
                    return '\x0D';
                case 'KeyN':
                    return '\x0E';
                case 'KeyO':
                    return '\x0F';
                case 'KeyP':
                    return '\x10';
                case 'KeyQ':
                    return '\x11';
                case 'KeyR':
                    return '\x12';
                case 'KeyS':
                    return '\x13';
                case 'KeyT':
                    return '\x14';
                case 'KeyU':
                    return '\x15';
                case 'KeyV':
                    return '\x16';
                case 'KeyW':
                    return '\x17';
                case 'KeyX':
                    return '\x18';
                case 'KeyY':
                    return '\x19';
                case 'KeyZ':
                    return '\x1A';
                case 'BracketLeft':
                    return '\x1B';
                case 'Backslash':
                    return '\x1C';
                case 'BracketRight':
                    return '\x1D';
                case 'Digit6':
                    if (source.shift) {
                        return '\x1E';
                    }
                    break;
                case 'Minus':
                    return '\x1F';
            }
            return '';
        }
        if (source.alt) {
            return '';
        }
        return;
    };
    function getCdpButton(button) {
        // https://www.w3.org/TR/pointerevents/#the-button-property
        switch (button) {
            case 0:
                return 'left';
            case 1:
                return 'middle';
            case 2:
                return 'right';
            case 3:
                return 'back';
            case 4:
                return 'forward';
            default:
                return 'none';
        }
    }
    function getTilt(action) {
        // https://w3c.github.io/pointerevents/#converting-between-tiltx-tilty-and-altitudeangle-azimuthangle
        const altitudeAngle = action.altitudeAngle ?? Math.PI / 2;
        const azimuthAngle = action.azimuthAngle ?? 0;
        let tiltXRadians = 0;
        let tiltYRadians = 0;
        if (altitudeAngle === 0) {
            // the pen is in the X-Y plane
            if (azimuthAngle === 0 || azimuthAngle === 2 * Math.PI) {
                // pen is on positive X axis
                tiltXRadians = Math.PI / 2;
            }
            if (azimuthAngle === Math.PI / 2) {
                // pen is on positive Y axis
                tiltYRadians = Math.PI / 2;
            }
            if (azimuthAngle === Math.PI) {
                // pen is on negative X axis
                tiltXRadians = -Math.PI / 2;
            }
            if (azimuthAngle === (3 * Math.PI) / 2) {
                // pen is on negative Y axis
                tiltYRadians = -Math.PI / 2;
            }
            if (azimuthAngle > 0 && azimuthAngle < Math.PI / 2) {
                tiltXRadians = Math.PI / 2;
                tiltYRadians = Math.PI / 2;
            }
            if (azimuthAngle > Math.PI / 2 && azimuthAngle < Math.PI) {
                tiltXRadians = -Math.PI / 2;
                tiltYRadians = Math.PI / 2;
            }
            if (azimuthAngle > Math.PI && azimuthAngle < (3 * Math.PI) / 2) {
                tiltXRadians = -Math.PI / 2;
                tiltYRadians = -Math.PI / 2;
            }
            if (azimuthAngle > (3 * Math.PI) / 2 && azimuthAngle < 2 * Math.PI) {
                tiltXRadians = Math.PI / 2;
                tiltYRadians = -Math.PI / 2;
            }
        }
        if (altitudeAngle !== 0) {
            const tanAlt = Math.tan(altitudeAngle);
            tiltXRadians = Math.atan(Math.cos(azimuthAngle) / tanAlt);
            tiltYRadians = Math.atan(Math.sin(azimuthAngle) / tanAlt);
        }
        const factor = 180 / Math.PI;
        return {
            tiltX: Math.round(tiltXRadians * factor),
            tiltY: Math.round(tiltYRadians * factor),
        };
    }
    function getRadii(width, height) {
        return {
            radiusX: width ? width / 2 : 0.5,
            radiusY: height ? height / 2 : 0.5,
        };
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     * Copyright 2022 The Chromium Authors.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Use Mutex class to coordinate local concurrent operations.
     * Once `acquire` promise resolves, you hold the lock and must
     * call `release` function returned by `acquire` to release the
     * lock. Failing to `release` the lock may lead to deadlocks.
     */
    class Mutex {
        #locked = false;
        #acquirers = [];
        // This is FIFO.
        acquire() {
            const state = { resolved: false };
            if (this.#locked) {
                return new Promise((resolve) => {
                    this.#acquirers.push(() => resolve(this.#release.bind(this, state)));
                });
            }
            this.#locked = true;
            return Promise.resolve(this.#release.bind(this, state));
        }
        #release(state) {
            if (state.resolved) {
                throw new Error('Cannot release more than once.');
            }
            state.resolved = true;
            const resolve = this.#acquirers.shift();
            if (!resolve) {
                this.#locked = false;
                return;
            }
            resolve();
        }
        async run(action) {
            const release = await this.acquire();
            try {
                // Note we need to await here because we want the await to release AFTER
                // that await happens. Returning action() will trigger the release
                // immediately which is counter to what we want.
                const result = await action();
                return result;
            }
            finally {
                release();
            }
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class InputState {
        cancelList = [];
        #sources = new Map();
        #mutex = new Mutex();
        getOrCreate(id, type, subtype) {
            let source = this.#sources.get(id);
            if (!source) {
                switch (type) {
                    case "none" /* SourceType.None */:
                        source = new NoneSource();
                        break;
                    case "key" /* SourceType.Key */:
                        source = new KeySource();
                        break;
                    case "pointer" /* SourceType.Pointer */: {
                        let pointerId = subtype === "mouse" /* Input.PointerType.Mouse */ ? 0 : 2;
                        const pointerIds = new Set();
                        for (const [, source] of this.#sources) {
                            if (source.type === "pointer" /* SourceType.Pointer */) {
                                pointerIds.add(source.pointerId);
                            }
                        }
                        while (pointerIds.has(pointerId)) {
                            ++pointerId;
                        }
                        source = new PointerSource(pointerId, subtype);
                        break;
                    }
                    case "wheel" /* SourceType.Wheel */:
                        source = new WheelSource();
                        break;
                    default:
                        throw new InvalidArgumentException(`Expected "${"none" /* SourceType.None */}", "${"key" /* SourceType.Key */}", "${"pointer" /* SourceType.Pointer */}", or "${"wheel" /* SourceType.Wheel */}". Found unknown source type ${type}.`);
                }
                this.#sources.set(id, source);
                return source;
            }
            if (source.type !== type) {
                throw new InvalidArgumentException(`Input source type of ${id} is ${source.type}, but received ${type}.`);
            }
            return source;
        }
        get(id) {
            const source = this.#sources.get(id);
            if (!source) {
                throw new UnknownErrorException(`Internal error.`);
            }
            return source;
        }
        getGlobalKeyState() {
            const state = new KeySource();
            for (const [, source] of this.#sources) {
                if (source.type !== "key" /* SourceType.Key */) {
                    continue;
                }
                for (const pressed of source.pressed) {
                    state.pressed.add(pressed);
                }
                state.alt ||= source.alt;
                state.ctrl ||= source.ctrl;
                state.meta ||= source.meta;
                state.shift ||= source.shift;
            }
            return state;
        }
        get queue() {
            return this.#mutex;
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    // We use a weak map here as specified here:
    // https://www.w3.org/TR/webdriver/#dfn-browsing-context-input-state-map
    class InputStateManager extends WeakMap {
        get(context) {
            assert(context.isTopLevelContext());
            if (!this.has(context)) {
                this.set(context, new InputState());
            }
            return super.get(context);
        }
    }

    /*
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class InputProcessor {
        #browsingContextStorage;
        #inputStateManager = new InputStateManager();
        constructor(browsingContextStorage) {
            this.#browsingContextStorage = browsingContextStorage;
        }
        async performActions(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            const inputState = this.#inputStateManager.get(context.top);
            const actionsByTick = this.#getActionsByTick(params, inputState);
            const dispatcher = new ActionDispatcher(inputState, context, await ActionDispatcher.isMacOS(context).catch(() => false));
            await dispatcher.dispatchActions(actionsByTick);
            return {};
        }
        async releaseActions(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            const topContext = context.top;
            const inputState = this.#inputStateManager.get(topContext);
            const dispatcher = new ActionDispatcher(inputState, context, await ActionDispatcher.isMacOS(context).catch(() => false));
            await dispatcher.dispatchTickActions(inputState.cancelList.reverse());
            this.#inputStateManager.delete(topContext);
            return {};
        }
        async setFiles(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            const realm = await context.getOrCreateSandbox(undefined);
            let result;
            try {
                result = await realm.callFunction(String(function getFiles(fileListLength) {
                    if (!(this instanceof HTMLInputElement)) {
                        if (this instanceof Element) {
                            return 1 /* ErrorCode.Element */;
                        }
                        return 0 /* ErrorCode.Node */;
                    }
                    if (this.type !== 'file') {
                        return 2 /* ErrorCode.Type */;
                    }
                    if (this.disabled) {
                        return 3 /* ErrorCode.Disabled */;
                    }
                    if (fileListLength > 1 && !this.multiple) {
                        return 4 /* ErrorCode.Multiple */;
                    }
                    return;
                }), false, params.element, [{ type: 'number', value: params.files.length }]);
            }
            catch {
                throw new NoSuchNodeException(`Could not find element ${params.element.sharedId}`);
            }
            assert(result.type === 'success');
            if (result.result.type === 'number') {
                switch (result.result.value) {
                    case 0 /* ErrorCode.Node */: {
                        throw new NoSuchElementException(`Could not find element ${params.element.sharedId}`);
                    }
                    case 1 /* ErrorCode.Element */: {
                        throw new UnableToSetFileInputException(`Element ${params.element.sharedId} is not a input`);
                    }
                    case 2 /* ErrorCode.Type */: {
                        throw new UnableToSetFileInputException(`Input element ${params.element.sharedId} is not a file type`);
                    }
                    case 3 /* ErrorCode.Disabled */: {
                        throw new UnableToSetFileInputException(`Input element ${params.element.sharedId} is disabled`);
                    }
                    case 4 /* ErrorCode.Multiple */: {
                        throw new UnableToSetFileInputException(`Cannot set multiple files on a non-multiple input element`);
                    }
                }
            }
            /**
             * The zero-length array is a special case, it seems that
             * DOM.setFileInputFiles does not actually update the files in that case, so
             * the solution is to eval the element value to a new FileList directly.
             */
            if (params.files.length === 0) {
                // XXX: These events should converted to trusted events. Perhaps do this
                // in `DOM.setFileInputFiles`?
                await realm.callFunction(String(function dispatchEvent() {
                    if (this.files?.length === 0) {
                        this.dispatchEvent(new Event('cancel', {
                            bubbles: true,
                        }));
                        return;
                    }
                    this.files = new DataTransfer().files;
                    // Dispatch events for this case because it should behave akin to a user action.
                    this.dispatchEvent(new Event('input', { bubbles: true, composed: true }));
                    this.dispatchEvent(new Event('change', { bubbles: true }));
                }), false, params.element);
                return {};
            }
            // Our goal here is to iterate over the input element files and get their
            // file paths.
            const paths = [];
            for (let i = 0; i < params.files.length; ++i) {
                const result = await realm.callFunction(String(function getFiles(index) {
                    return this.files?.item(index);
                }), false, params.element, [{ type: 'number', value: 0 }], "root" /* Script.ResultOwnership.Root */);
                assert(result.type === 'success');
                if (result.result.type !== 'object') {
                    break;
                }
                const { handle } = result.result;
                assert(handle !== undefined);
                const { path } = await realm.cdpClient.sendCommand('DOM.getFileInfo', {
                    objectId: handle,
                });
                paths.push(path);
                // Cleanup the handle.
                void realm.disown(handle).catch(undefined);
            }
            paths.sort();
            // We create a new array so we preserve the order of the original files.
            const sortedFiles = [...params.files].sort();
            if (paths.length !== params.files.length ||
                sortedFiles.some((path, index) => {
                    return paths[index] !== path;
                })) {
                const { objectId } = await realm.deserializeForCdp(params.element);
                // This cannot throw since this was just used in `callFunction` above.
                assert(objectId !== undefined);
                await realm.cdpClient.sendCommand('DOM.setFileInputFiles', {
                    files: params.files,
                    objectId,
                });
            }
            else {
                // XXX: We should dispatch a trusted event.
                await realm.callFunction(String(function dispatchEvent() {
                    this.dispatchEvent(new Event('cancel', {
                        bubbles: true,
                    }));
                }), false, params.element);
            }
            return {};
        }
        #getActionsByTick(params, inputState) {
            const actionsByTick = [];
            for (const action of params.actions) {
                switch (action.type) {
                    case "pointer" /* SourceType.Pointer */: {
                        action.parameters ??= { pointerType: "mouse" /* Input.PointerType.Mouse */ };
                        action.parameters.pointerType ??= "mouse" /* Input.PointerType.Mouse */;
                        const source = inputState.getOrCreate(action.id, "pointer" /* SourceType.Pointer */, action.parameters.pointerType);
                        if (source.subtype !== action.parameters.pointerType) {
                            throw new InvalidArgumentException(`Expected input source ${action.id} to be ${source.subtype}; got ${action.parameters.pointerType}.`);
                        }
                        break;
                    }
                    default:
                        inputState.getOrCreate(action.id, action.type);
                }
                const actions = action.actions.map((item) => ({
                    id: action.id,
                    action: item,
                }));
                for (let i = 0; i < actions.length; i++) {
                    if (actionsByTick.length === i) {
                        actionsByTick.push([]);
                    }
                    actionsByTick[i].push(actions[i]);
                }
            }
            return actionsByTick;
        }
    }

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    const URLPattern = globalThis.URLPattern;
    if (!URLPattern) {
        throw new Error('Unable to find URLPattern');
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** Dispatches Network domain commands. */
    class NetworkProcessor {
        #browsingContextStorage;
        #networkStorage;
        constructor(browsingContextStorage, networkStorage) {
            this.#browsingContextStorage = browsingContextStorage;
            this.#networkStorage = networkStorage;
        }
        async addIntercept(params) {
            this.#browsingContextStorage.verifyTopLevelContextsList(params.contexts);
            const urlPatterns = params.urlPatterns ?? [];
            const parsedUrlPatterns = NetworkProcessor.parseUrlPatterns(urlPatterns);
            const intercept = this.#networkStorage.addIntercept({
                urlPatterns: parsedUrlPatterns,
                phases: params.phases,
                contexts: params.contexts,
            });
            await Promise.all(this.#browsingContextStorage.getAllContexts().map((context) => {
                return context.cdpTarget.toggleFetchIfNeeded();
            }));
            return {
                intercept,
            };
        }
        async continueRequest(params) {
            if (params.url !== undefined) {
                NetworkProcessor.parseUrlString(params.url);
            }
            if (params.method !== undefined) {
                if (!NetworkProcessor.isMethodValid(params.method)) {
                    throw new InvalidArgumentException(`Method '${params.method}' is invalid.`);
                }
            }
            if (params.headers) {
                NetworkProcessor.validateHeaders(params.headers);
            }
            const request = this.#getBlockedRequestOrFail(params.request, [
                "beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */,
            ]);
            try {
                await request.continueRequest(params);
            }
            catch (error) {
                throw NetworkProcessor.wrapInterceptionError(error);
            }
            return {};
        }
        async continueResponse(params) {
            if (params.headers) {
                NetworkProcessor.validateHeaders(params.headers);
            }
            const request = this.#getBlockedRequestOrFail(params.request, [
                "authRequired" /* Network.InterceptPhase.AuthRequired */,
                "responseStarted" /* Network.InterceptPhase.ResponseStarted */,
            ]);
            try {
                await request.continueResponse(params);
            }
            catch (error) {
                throw NetworkProcessor.wrapInterceptionError(error);
            }
            return {};
        }
        async continueWithAuth(params) {
            const networkId = params.request;
            const request = this.#getBlockedRequestOrFail(networkId, [
                "authRequired" /* Network.InterceptPhase.AuthRequired */,
            ]);
            await request.continueWithAuth(params);
            return {};
        }
        async failRequest({ request: networkId, }) {
            const request = this.#getRequestOrFail(networkId);
            if (request.interceptPhase === "authRequired" /* Network.InterceptPhase.AuthRequired */) {
                throw new InvalidArgumentException(`Request '${networkId}' in 'authRequired' phase cannot be failed`);
            }
            if (!request.interceptPhase) {
                throw new NoSuchRequestException(`No blocked request found for network id '${networkId}'`);
            }
            await request.failRequest('Failed');
            return {};
        }
        async provideResponse(params) {
            if (params.headers) {
                NetworkProcessor.validateHeaders(params.headers);
            }
            const request = this.#getBlockedRequestOrFail(params.request, [
                "beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */,
                "responseStarted" /* Network.InterceptPhase.ResponseStarted */,
                "authRequired" /* Network.InterceptPhase.AuthRequired */,
            ]);
            try {
                await request.provideResponse(params);
            }
            catch (error) {
                throw NetworkProcessor.wrapInterceptionError(error);
            }
            return {};
        }
        async removeIntercept(params) {
            this.#networkStorage.removeIntercept(params.intercept);
            await Promise.all(this.#browsingContextStorage.getAllContexts().map((context) => {
                return context.cdpTarget.toggleFetchIfNeeded();
            }));
            return {};
        }
        async setCacheBehavior(params) {
            const contexts = this.#browsingContextStorage.verifyTopLevelContextsList(params.contexts);
            // Change all targets
            if (contexts.size === 0) {
                this.#networkStorage.defaultCacheBehavior = params.cacheBehavior;
                await Promise.all(this.#browsingContextStorage.getAllContexts().map((context) => {
                    return context.cdpTarget.toggleSetCacheDisabled();
                }));
                return {};
            }
            const cacheDisabled = params.cacheBehavior === 'bypass';
            await Promise.all([...contexts.values()].map((context) => {
                return context.cdpTarget.toggleSetCacheDisabled(cacheDisabled);
            }));
            return {};
        }
        #getRequestOrFail(id) {
            const request = this.#networkStorage.getRequestById(id);
            if (!request) {
                throw new NoSuchRequestException(`Network request with ID '${id}' doesn't exist`);
            }
            return request;
        }
        #getBlockedRequestOrFail(id, phases) {
            const request = this.#getRequestOrFail(id);
            if (!request.interceptPhase) {
                throw new NoSuchRequestException(`No blocked request found for network id '${id}'`);
            }
            if (request.interceptPhase && !phases.includes(request.interceptPhase)) {
                throw new InvalidArgumentException(`Blocked request for network id '${id}' is in '${request.interceptPhase}' phase`);
            }
            return request;
        }
        /**
         * Validate https://fetch.spec.whatwg.org/#header-value
         */
        static validateHeaders(headers) {
            for (const header of headers) {
                let headerValue;
                if (header.value.type === 'string') {
                    headerValue = header.value.value;
                }
                else {
                    headerValue = atob(header.value.value);
                }
                if (headerValue !== headerValue.trim() ||
                    headerValue.includes('\n') ||
                    headerValue.includes('\0')) {
                    throw new InvalidArgumentException(`Header value '${headerValue}' is not acceptable value`);
                }
            }
        }
        static isMethodValid(method) {
            // https://httpwg.org/specs/rfc9110.html#method.overview
            return /^[!#$%&'*+\-.^_`|~a-zA-Z\d]+$/.test(method);
        }
        /**
         * Attempts to parse the given url.
         * Throws an InvalidArgumentException if the url is invalid.
         */
        static parseUrlString(url) {
            try {
                return new URL(url);
            }
            catch (error) {
                throw new InvalidArgumentException(`Invalid URL '${url}': ${error}`);
            }
        }
        static parseUrlPatterns(urlPatterns) {
            return urlPatterns.map((urlPattern) => {
                switch (urlPattern.type) {
                    case 'string': {
                        NetworkProcessor.parseUrlString(urlPattern.pattern);
                        return urlPattern;
                    }
                    case 'pattern':
                        // No params signifies intercept all
                        if (urlPattern.protocol === undefined &&
                            urlPattern.hostname === undefined &&
                            urlPattern.port === undefined &&
                            urlPattern.pathname === undefined &&
                            urlPattern.search === undefined) {
                            return urlPattern;
                        }
                        if (urlPattern.protocol) {
                            urlPattern.protocol = unescapeURLPattern(urlPattern.protocol);
                            if (!urlPattern.protocol.match(/^[a-zA-Z+-.]+$/)) {
                                throw new InvalidArgumentException('Forbidden characters');
                            }
                        }
                        if (urlPattern.hostname) {
                            urlPattern.hostname = unescapeURLPattern(urlPattern.hostname);
                        }
                        if (urlPattern.port) {
                            urlPattern.port = unescapeURLPattern(urlPattern.port);
                        }
                        if (urlPattern.pathname) {
                            urlPattern.pathname = unescapeURLPattern(urlPattern.pathname);
                            if (urlPattern.pathname[0] !== '/') {
                                urlPattern.pathname = `/${urlPattern.pathname}`;
                            }
                            if (urlPattern.pathname.includes('#') ||
                                urlPattern.pathname.includes('?')) {
                                throw new InvalidArgumentException('Forbidden characters');
                            }
                        }
                        else if (urlPattern.pathname === '') {
                            urlPattern.pathname = '/';
                        }
                        if (urlPattern.search) {
                            urlPattern.search = unescapeURLPattern(urlPattern.search);
                            if (urlPattern.search[0] !== '?') {
                                urlPattern.search = `?${urlPattern.search}`;
                            }
                            if (urlPattern.search.includes('#')) {
                                throw new InvalidArgumentException('Forbidden characters');
                            }
                        }
                        if (urlPattern.protocol === '') {
                            throw new InvalidArgumentException(`URL pattern must specify a protocol`);
                        }
                        if (urlPattern.hostname === '') {
                            throw new InvalidArgumentException(`URL pattern must specify a hostname`);
                        }
                        if ((urlPattern.hostname?.length ?? 0) > 0) {
                            if (urlPattern.protocol?.match(/^file/i)) {
                                throw new InvalidArgumentException(`URL pattern protocol cannot be 'file'`);
                            }
                            if (urlPattern.hostname?.includes(':')) {
                                throw new InvalidArgumentException(`URL pattern hostname must not contain a colon`);
                            }
                        }
                        if (urlPattern.port === '') {
                            throw new InvalidArgumentException(`URL pattern must specify a port`);
                        }
                        try {
                            new URLPattern(urlPattern);
                        }
                        catch (error) {
                            throw new InvalidArgumentException(`${error}`);
                        }
                        return urlPattern;
                }
            });
        }
        static wrapInterceptionError(error) {
            // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/fetch_handler.cc;l=169
            if (error?.message.includes('Invalid header')) {
                return new InvalidArgumentException('Invalid header');
            }
            return error;
        }
    }
    /**
     * See https://w3c.github.io/webdriver-bidi/#unescape-url-pattern
     */
    function unescapeURLPattern(pattern) {
        const forbidden = new Set(['(', ')', '*', '{', '}']);
        let result = '';
        let isEscaped = false;
        for (const c of pattern) {
            if (!isEscaped) {
                if (forbidden.has(c)) {
                    throw new InvalidArgumentException('Forbidden characters');
                }
                if (c === '\\') {
                    isEscaped = true;
                    continue;
                }
            }
            result += c;
            isEscaped = false;
        }
        return result;
    }

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class PermissionsProcessor {
        #browserCdpClient;
        constructor(browserCdpClient) {
            this.#browserCdpClient = browserCdpClient;
        }
        async setPermissions(params) {
            try {
                const userContextId = params['goog:userContext'] ||
                    params.userContext;
                await this.#browserCdpClient.sendCommand('Browser.setPermission', {
                    origin: params.origin,
                    browserContextId: userContextId && userContextId !== 'default'
                        ? userContextId
                        : undefined,
                    permission: {
                        name: params.descriptor.name,
                    },
                    setting: params.state,
                });
            }
            catch (err) {
                if (err.message ===
                    `Permission can't be granted to opaque origins.`) {
                    // Return success if the origin is not valid (does not match any
                    // existing origins).
                    return {};
                }
                throw new InvalidArgumentException(err.message);
            }
            return {};
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    function bytesToHex(bytes) {
        return bytes.reduce((str, byte) => str + byte.toString(16).padStart(2, '0'), '');
    }
    /**
     * Generates a random v4 UUID, as specified in RFC4122.
     *
     * Uses the native Web Crypto API if available, otherwise falls back to a
     * polyfill.
     *
     * Example: '9b1deb4d-3b7d-4bad-9bdd-2b0d7b3dcb6d'
     */
    function uuidv4() {
        // Available only in secure contexts
        // https://developer.mozilla.org/en-US/docs/Web/API/Web_Crypto_API
        if ('crypto' in globalThis && 'randomUUID' in globalThis.crypto) {
            // Node with
            // https://nodejs.org/dist/latest-v20.x/docs/api/globals.html#crypto_1 or
            // secure browser context.
            return globalThis.crypto.randomUUID();
        }
        const randomValues = new Uint8Array(16);
        if ('crypto' in globalThis && 'getRandomValues' in globalThis.crypto) {
            // Node (>=18) with
            // https://nodejs.org/dist/latest-v20.x/docs/api/globals.html#crypto_1 or
            // browser.
            globalThis.crypto.getRandomValues(randomValues);
        }
        else {
            // Node (<=16) without
            // https://nodejs.org/dist/latest-v20.x/docs/api/globals.html#crypto_1.
            // eslint-disable-next-line @typescript-eslint/no-var-requires,@typescript-eslint/no-require-imports
            require('crypto').webcrypto.getRandomValues(randomValues);
        }
        // Set version (4) and variant (RFC4122) bits.
        randomValues[6] = (randomValues[6] & 0x0f) | 0x40;
        randomValues[8] = (randomValues[8] & 0x3f) | 0x80;
        return [
            bytesToHex(randomValues.subarray(0, 4)),
            bytesToHex(randomValues.subarray(4, 6)),
            bytesToHex(randomValues.subarray(6, 8)),
            bytesToHex(randomValues.subarray(8, 10)),
            bytesToHex(randomValues.subarray(10, 16)),
        ].join('-');
    }

    /*
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     *
     */
    /**
     * Used to send messages from realm to BiDi user.
     */
    class ChannelProxy {
        #properties;
        #id = uuidv4();
        #logger;
        constructor(channel, logger) {
            this.#properties = channel;
            this.#logger = logger;
        }
        /**
         * Creates a channel proxy in the given realm, initialises listener and
         * returns a handle to `sendMessage` delegate.
         */
        async init(realm, eventManager) {
            const channelHandle = await ChannelProxy.#createAndGetHandleInRealm(realm);
            const sendMessageHandle = await ChannelProxy.#createSendMessageHandle(realm, channelHandle);
            void this.#startListener(realm, channelHandle, eventManager);
            return sendMessageHandle;
        }
        /** Gets a ChannelProxy from window and returns its handle. */
        async startListenerFromWindow(realm, eventManager) {
            try {
                const channelHandle = await this.#getHandleFromWindow(realm);
                void this.#startListener(realm, channelHandle, eventManager);
            }
            catch (error) {
                this.#logger?.(LogType.debugError, error);
            }
        }
        /**
         * Evaluation string which creates a ChannelProxy object on the client side.
         */
        static #createChannelProxyEvalStr() {
            const functionStr = String(() => {
                const queue = [];
                let queueNonEmptyResolver = null;
                return {
                    /**
                     * Gets a promise, which is resolved as soon as a message occurs
                     * in the queue.
                     */
                    async getMessage() {
                        const onMessage = queue.length > 0
                            ? Promise.resolve()
                            : new Promise((resolve) => {
                                queueNonEmptyResolver = resolve;
                            });
                        await onMessage;
                        return queue.shift();
                    },
                    /**
                     * Adds a message to the queue.
                     * Resolves the pending promise if needed.
                     */
                    sendMessage(message) {
                        queue.push(message);
                        if (queueNonEmptyResolver !== null) {
                            queueNonEmptyResolver();
                            queueNonEmptyResolver = null;
                        }
                    },
                };
            });
            return `(${functionStr})()`;
        }
        /** Creates a ChannelProxy in the given realm. */
        static async #createAndGetHandleInRealm(realm) {
            const createChannelHandleResult = await realm.cdpClient.sendCommand('Runtime.evaluate', {
                expression: this.#createChannelProxyEvalStr(),
                contextId: realm.executionContextId,
                serializationOptions: {
                    serialization: "idOnly" /* Protocol.Runtime.SerializationOptionsSerialization.IdOnly */,
                },
            });
            if (createChannelHandleResult.exceptionDetails ||
                createChannelHandleResult.result.objectId === undefined) {
                throw new Error(`Cannot create channel`);
            }
            return createChannelHandleResult.result.objectId;
        }
        /** Gets a handle to `sendMessage` delegate from the ChannelProxy handle. */
        static async #createSendMessageHandle(realm, channelHandle) {
            const sendMessageArgResult = await realm.cdpClient.sendCommand('Runtime.callFunctionOn', {
                functionDeclaration: String((channelHandle) => {
                    return channelHandle.sendMessage;
                }),
                arguments: [{ objectId: channelHandle }],
                executionContextId: realm.executionContextId,
                serializationOptions: {
                    serialization: "idOnly" /* Protocol.Runtime.SerializationOptionsSerialization.IdOnly */,
                },
            });
            // TODO: check for exceptionDetails.
            return sendMessageArgResult.result.objectId;
        }
        /** Starts listening for the channel events of the provided ChannelProxy. */
        async #startListener(realm, channelHandle, eventManager) {
            // noinspection InfiniteLoopJS
            for (;;) {
                try {
                    const message = await realm.cdpClient.sendCommand('Runtime.callFunctionOn', {
                        functionDeclaration: String(async (channelHandle) => await channelHandle.getMessage()),
                        arguments: [
                            {
                                objectId: channelHandle,
                            },
                        ],
                        awaitPromise: true,
                        executionContextId: realm.executionContextId,
                        serializationOptions: {
                            serialization: "deep" /* Protocol.Runtime.SerializationOptionsSerialization.Deep */,
                            maxDepth: this.#properties.serializationOptions?.maxObjectDepth ??
                                undefined,
                        },
                    });
                    if (message.exceptionDetails) {
                        throw new Error('Runtime.callFunctionOn in ChannelProxy', {
                            cause: message.exceptionDetails,
                        });
                    }
                    for (const browsingContext of realm.associatedBrowsingContexts) {
                        eventManager.registerEvent({
                            type: 'event',
                            method: Script$2.EventNames.Message,
                            params: {
                                channel: this.#properties.channel,
                                data: realm.cdpToBidiValue(message, this.#properties.ownership ?? "none" /* Script.ResultOwnership.None */),
                                source: realm.source,
                            },
                        }, browsingContext.id);
                    }
                }
                catch (error) {
                    // If an error is thrown, then the channel is permanently broken, so we
                    // exit the loop.
                    this.#logger?.(LogType.debugError, error);
                    break;
                }
            }
        }
        /**
         * Returns a handle of ChannelProxy from window's property which was set there
         * by `getEvalInWindowStr`. If window property is not set yet, sets a promise
         * resolver to the window property, so that `getEvalInWindowStr` can resolve
         * the promise later on with the channel.
         * This is needed because `getEvalInWindowStr` can be called before or
         * after this method.
         */
        async #getHandleFromWindow(realm) {
            const channelHandleResult = await realm.cdpClient.sendCommand('Runtime.callFunctionOn', {
                functionDeclaration: String((id) => {
                    const w = window;
                    if (w[id] === undefined) {
                        // The channelProxy is not created yet. Create a promise, put the
                        // resolver to window property and return the promise.
                        // `getEvalInWindowStr` will resolve the promise later.
                        return new Promise((resolve) => (w[id] = resolve));
                    }
                    // The channelProxy is already created by `getEvalInWindowStr` and
                    // is set into window property. Return it.
                    const channelProxy = w[id];
                    delete w[id];
                    return channelProxy;
                }),
                arguments: [{ value: this.#id }],
                executionContextId: realm.executionContextId,
                awaitPromise: true,
                serializationOptions: {
                    serialization: "idOnly" /* Protocol.Runtime.SerializationOptionsSerialization.IdOnly */,
                },
            });
            if (channelHandleResult.exceptionDetails !== undefined ||
                channelHandleResult.result.objectId === undefined) {
                throw new Error(`ChannelHandle not found in window["${this.#id}"]`);
            }
            return channelHandleResult.result.objectId;
        }
        /**
         * String to be evaluated to create a ProxyChannel and put it to window.
         * Returns the delegate `sendMessage`. Used to provide an argument for preload
         * script. Does the following:
         * 1. Creates a ChannelProxy.
         * 2. Puts the ChannelProxy to window['${this.#id}'] or resolves the promise
         *    by calling delegate stored in window['${this.#id}'].
         *    This is needed because `#getHandleFromWindow` can be called before or
         *    after this method.
         * 3. Returns the delegate `sendMessage` of the created ChannelProxy.
         */
        getEvalInWindowStr() {
            const delegate = String((id, channelProxy) => {
                const w = window;
                if (w[id] === undefined) {
                    // `#getHandleFromWindow` is not initialized yet, and will get the
                    // channelProxy later.
                    w[id] = channelProxy;
                }
                else {
                    // `#getHandleFromWindow` is already set a delegate to window property
                    // and is waiting for it to be called with the channelProxy.
                    w[id](channelProxy);
                    delete w[id];
                }
                return channelProxy.sendMessage;
            });
            const channelProxyEval = ChannelProxy.#createChannelProxyEvalStr();
            return `(${delegate})('${this.#id}',${channelProxyEval})`;
        }
    }

    /*
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     *
     */
    /**
     * BiDi IDs are generated by the server and are unique within contexts.
     *
     * CDP preload script IDs are generated by the client and are unique
     * within sessions.
     *
     * The mapping between BiDi and CDP preload script IDs is 1:many.
     * BiDi IDs are needed by the mapper to keep track of potential multiple CDP IDs
     * in the client.
     */
    class PreloadScript {
        /** BiDi ID, an automatically generated UUID. */
        #id = uuidv4();
        /** CDP preload scripts. */
        #cdpPreloadScripts = [];
        /** The script itself, in a format expected by the spec i.e. a function. */
        #functionDeclaration;
        /** Targets, in which the preload script is initialized. */
        #targetIds = new Set();
        /** Channels to be added as arguments to functionDeclaration. */
        #channels;
        /** The script sandbox / world name. */
        #sandbox;
        /** The browsing contexts to execute the preload scripts in, if any. */
        #contexts;
        get id() {
            return this.#id;
        }
        get targetIds() {
            return this.#targetIds;
        }
        constructor(params, logger) {
            this.#channels =
                params.arguments?.map((a) => new ChannelProxy(a.value, logger)) ?? [];
            this.#functionDeclaration = params.functionDeclaration;
            this.#sandbox = params.sandbox;
            this.#contexts = params.contexts;
        }
        /** Channels of the preload script. */
        get channels() {
            return this.#channels;
        }
        /** Contexts of the preload script, if any */
        get contexts() {
            return this.#contexts;
        }
        /**
         * String to be evaluated. Wraps user-provided function so that the following
         * steps are run:
         * 1. Create channels.
         * 2. Store the created channels in window.
         * 3. Call the user-provided function with channels as arguments.
         */
        #getEvaluateString() {
            const channelsArgStr = `[${this.channels
            .map((c) => c.getEvalInWindowStr())
            .join(', ')}]`;
            return `(()=>{(${this.#functionDeclaration})(...${channelsArgStr})})()`;
        }
        /**
         * Adds the script to the given CDP targets by calling the
         * `Page.addScriptToEvaluateOnNewDocument` command.
         */
        async initInTargets(cdpTargets, runImmediately) {
            await Promise.all(Array.from(cdpTargets).map((cdpTarget) => this.initInTarget(cdpTarget, runImmediately)));
        }
        /**
         * Adds the script to the given CDP target by calling the
         * `Page.addScriptToEvaluateOnNewDocument` command.
         */
        async initInTarget(cdpTarget, runImmediately) {
            const addCdpPreloadScriptResult = await cdpTarget.cdpClient.sendCommand('Page.addScriptToEvaluateOnNewDocument', {
                source: this.#getEvaluateString(),
                worldName: this.#sandbox,
                runImmediately,
            });
            this.#cdpPreloadScripts.push({
                target: cdpTarget,
                preloadScriptId: addCdpPreloadScriptResult.identifier,
            });
            this.#targetIds.add(cdpTarget.id);
        }
        /**
         * Removes this script from all CDP targets.
         */
        async remove() {
            await Promise.all([
                this.#cdpPreloadScripts.map(async (cdpPreloadScript) => {
                    const cdpTarget = cdpPreloadScript.target;
                    const cdpPreloadScriptId = cdpPreloadScript.preloadScriptId;
                    return await cdpTarget.cdpClient.sendCommand('Page.removeScriptToEvaluateOnNewDocument', {
                        identifier: cdpPreloadScriptId,
                    });
                }),
            ]);
        }
        /** Removes the provided cdp target from the list of cdp preload scripts. */
        dispose(cdpTargetId) {
            this.#cdpPreloadScripts = this.#cdpPreloadScripts.filter((cdpPreloadScript) => cdpPreloadScript.target?.id !== cdpTargetId);
            this.#targetIds.delete(cdpTargetId);
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class ScriptProcessor {
        #eventManager;
        #browsingContextStorage;
        #realmStorage;
        #preloadScriptStorage;
        #logger;
        constructor(eventManager, browsingContextStorage, realmStorage, preloadScriptStorage, logger) {
            this.#browsingContextStorage = browsingContextStorage;
            this.#realmStorage = realmStorage;
            this.#preloadScriptStorage = preloadScriptStorage;
            this.#logger = logger;
            this.#eventManager = eventManager;
            this.#eventManager.addSubscribeHook(Script$2.EventNames.RealmCreated, this.#onRealmCreatedSubscribeHook.bind(this));
        }
        #onRealmCreatedSubscribeHook(contextId) {
            const context = this.#browsingContextStorage.getContext(contextId);
            const contextsToReport = [
                context,
                ...this.#browsingContextStorage.getContext(contextId).allChildren,
            ];
            const realms = new Set();
            for (const reportContext of contextsToReport) {
                const realmsForContext = this.#realmStorage.findRealms({
                    browsingContextId: reportContext.id,
                });
                for (const realm of realmsForContext) {
                    realms.add(realm);
                }
            }
            for (const realm of realms) {
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: Script$2.EventNames.RealmCreated,
                    params: realm.realmInfo,
                }, context.id);
            }
            return Promise.resolve();
        }
        async addPreloadScript(params) {
            const contexts = this.#browsingContextStorage.verifyTopLevelContextsList(params.contexts);
            const preloadScript = new PreloadScript(params, this.#logger);
            this.#preloadScriptStorage.add(preloadScript);
            const cdpTargets = contexts.size === 0
                ? new Set(this.#browsingContextStorage
                    .getTopLevelContexts()
                    .map((context) => context.cdpTarget))
                : new Set([...contexts.values()].map((context) => context.cdpTarget));
            await preloadScript.initInTargets(cdpTargets, false);
            return {
                script: preloadScript.id,
            };
        }
        async removePreloadScript(params) {
            const { script: id } = params;
            const scripts = this.#preloadScriptStorage.find({ id });
            if (scripts.length === 0) {
                throw new NoSuchScriptException(`No preload script with id '${id}'`);
            }
            await Promise.all(scripts.map((script) => script.remove()));
            this.#preloadScriptStorage.remove({ id });
            return {};
        }
        async callFunction(params) {
            const realm = await this.#getRealm(params.target);
            return await realm.callFunction(params.functionDeclaration, params.awaitPromise, params.this, params.arguments, params.resultOwnership, params.serializationOptions, params.userActivation);
        }
        async evaluate(params) {
            const realm = await this.#getRealm(params.target);
            return await realm.evaluate(params.expression, params.awaitPromise, params.resultOwnership, params.serializationOptions, params.userActivation);
        }
        async disown(params) {
            const realm = await this.#getRealm(params.target);
            await Promise.all(params.handles.map(async (handle) => await realm.disown(handle)));
            return {};
        }
        getRealms(params) {
            if (params.context !== undefined) {
                // Make sure the context is known.
                this.#browsingContextStorage.getContext(params.context);
            }
            const realms = this.#realmStorage
                .findRealms({
                browsingContextId: params.context,
                type: params.type,
            })
                .map((realm) => realm.realmInfo);
            return { realms };
        }
        async #getRealm(target) {
            if ('context' in target) {
                const context = this.#browsingContextStorage.getContext(target.context);
                return await context.getOrCreateSandbox(target.sandbox);
            }
            return this.#realmStorage.getRealm({
                realmId: target.realm,
            });
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class SessionProcessor {
        #eventManager;
        #browserCdpClient;
        #initConnection;
        #created = false;
        constructor(eventManager, browserCdpClient, initConnection) {
            this.#eventManager = eventManager;
            this.#browserCdpClient = browserCdpClient;
            this.#initConnection = initConnection;
        }
        status() {
            return { ready: false, message: 'already connected' };
        }
        #mergeCapabilities(capabilitiesRequest) {
            // Roughly following https://www.w3.org/TR/webdriver2/#dfn-capabilities-processing.
            // Validations should already be done by the parser.
            const mergedCapabilities = [];
            for (const first of capabilitiesRequest.firstMatch ?? [{}]) {
                const result = {
                    ...capabilitiesRequest.alwaysMatch,
                };
                for (const key of Object.keys(first)) {
                    if (result[key] !== undefined) {
                        throw new InvalidArgumentException(`Capability ${key} in firstMatch is already defined in alwaysMatch`);
                    }
                    result[key] = first[key];
                }
                mergedCapabilities.push(result);
            }
            const match = mergedCapabilities.find((c) => c.browserName === 'chrome') ??
                mergedCapabilities[0] ??
                {};
            match.unhandledPromptBehavior = this.#getUnhandledPromptBehavior(match.unhandledPromptBehavior);
            return match;
        }
        #getUnhandledPromptBehavior(capabilityValue) {
            if (capabilityValue === undefined) {
                return undefined;
            }
            if (typeof capabilityValue === 'object') {
                // Do not validate capabilities. Incorrect ones will be ignored by Mapper.
                return capabilityValue;
            }
            if (typeof capabilityValue !== 'string') {
                throw new InvalidArgumentException(`Unexpected 'unhandledPromptBehavior' type: ${typeof capabilityValue}`);
            }
            switch (capabilityValue) {
                case 'accept':
                case 'accept and notify':
                    return { default: "accept" /* Session.UserPromptHandlerType.Accept */ };
                case 'dismiss':
                case 'dismiss and notify':
                    return { default: "dismiss" /* Session.UserPromptHandlerType.Dismiss */ };
                case 'ignore':
                    return { default: "ignore" /* Session.UserPromptHandlerType.Ignore */ };
                default:
                    throw new InvalidArgumentException(`Unexpected 'unhandledPromptBehavior' value: ${capabilityValue}`);
            }
        }
        async new(params) {
            if (this.#created) {
                throw new Error('Session has been already created.');
            }
            this.#created = true;
            const matchedCapabitlites = this.#mergeCapabilities(params.capabilities);
            await this.#initConnection(matchedCapabitlites);
            const version = await this.#browserCdpClient.sendCommand('Browser.getVersion');
            return {
                sessionId: 'unknown',
                capabilities: {
                    ...matchedCapabitlites,
                    acceptInsecureCerts: matchedCapabitlites.acceptInsecureCerts ?? false,
                    browserName: version.product,
                    browserVersion: version.revision,
                    platformName: '',
                    setWindowRect: false,
                    webSocketUrl: '',
                    userAgent: version.userAgent,
                },
            };
        }
        async subscribe(params, channel = null) {
            await this.#eventManager.subscribe(params.events, params.contexts ?? [null], channel);
            return {};
        }
        async unsubscribe(params, channel = null) {
            await this.#eventManager.unsubscribe(params.events, params.contexts ?? [null], channel);
            return {};
        }
    }

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Encodes a string to base64.
     *
     * Uses the native Web API if available, otherwise falls back to a NodeJS Buffer.
     * @param {string} base64Str
     * @return {string}
     */
    function base64ToString(base64Str) {
        // Available only if run in a browser context.
        if ('atob' in globalThis) {
            return globalThis.atob(base64Str);
        }
        // Available only if run in a NodeJS context.
        return Buffer.from(base64Str, 'base64').toString('ascii');
    }

    /*
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     *
     */
    function computeHeadersSize(headers) {
        const requestHeaders = headers.reduce((acc, header) => {
            return `${acc}${header.name}: ${header.value.value}\r\n`;
        }, '');
        return new TextEncoder().encode(requestHeaders).length;
    }
    /** Converts from CDP Network domain headers to BiDi network headers. */
    function bidiNetworkHeadersFromCdpNetworkHeaders(headers) {
        if (!headers) {
            return [];
        }
        return Object.entries(headers).map(([name, value]) => ({
            name,
            value: {
                type: 'string',
                value,
            },
        }));
    }
    /** Converts from Bidi network headers to CDP Fetch domain header entries. */
    function cdpFetchHeadersFromBidiNetworkHeaders(headers) {
        if (headers === undefined) {
            return undefined;
        }
        return headers.map(({ name, value }) => ({
            name,
            value: value.value,
        }));
    }
    function networkHeaderFromCookieHeaders(headers) {
        if (headers === undefined) {
            return undefined;
        }
        const value = headers.reduce((acc, value, index) => {
            if (index > 0) {
                acc += ';';
            }
            const cookieValue = value.value.type === 'base64'
                ? btoa(value.value.value)
                : value.value.value;
            acc += `${value.name}=${cookieValue}`;
            return acc;
        }, '');
        return {
            name: 'Cookie',
            value: {
                type: 'string',
                value,
            },
        };
    }
    /** Converts from Bidi auth action to CDP auth challenge response. */
    function cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction(action) {
        switch (action) {
            case 'default':
                return 'Default';
            case 'cancel':
                return 'CancelAuth';
            case 'provideCredentials':
                return 'ProvideCredentials';
        }
    }
    /**
     * Converts from CDP Network domain cookie to BiDi network cookie.
     * * https://chromedevtools.github.io/devtools-protocol/tot/Network/#type-Cookie
     * * https://w3c.github.io/webdriver-bidi/#type-network-Cookie
     */
    function cdpToBiDiCookie(cookie) {
        const result = {
            name: cookie.name,
            value: { type: 'string', value: cookie.value },
            domain: cookie.domain,
            path: cookie.path,
            size: cookie.size,
            httpOnly: cookie.httpOnly,
            secure: cookie.secure,
            sameSite: cookie.sameSite === undefined
                ? "none" /* Network.SameSite.None */
                : sameSiteCdpToBiDi(cookie.sameSite),
            ...(cookie.expires >= 0 ? { expiry: cookie.expires } : undefined),
        };
        // Extending with CDP-specific properties with `goog:` prefix.
        result[`goog:session`] = cookie.session;
        result[`goog:priority`] = cookie.priority;
        result[`goog:sameParty`] = cookie.sameParty;
        result[`goog:sourceScheme`] = cookie.sourceScheme;
        result[`goog:sourcePort`] = cookie.sourcePort;
        if (cookie.partitionKey !== undefined) {
            result[`goog:partitionKey`] = cookie.partitionKey;
        }
        if (cookie.partitionKeyOpaque !== undefined) {
            result[`goog:partitionKeyOpaque`] = cookie.partitionKeyOpaque;
        }
        return result;
    }
    /**
     * Decodes a byte value to a string.
     * @param {Network.BytesValue} value
     * @return {string}
     */
    function deserializeByteValue(value) {
        if (value.type === 'base64') {
            return base64ToString(value.value);
        }
        return value.value;
    }
    /**
     * Converts from BiDi set network cookie params to CDP Network domain cookie.
     * * https://w3c.github.io/webdriver-bidi/#type-network-Cookie
     * * https://chromedevtools.github.io/devtools-protocol/tot/Network/#type-CookieParam
     */
    function bidiToCdpCookie(params, partitionKey) {
        const deserializedValue = deserializeByteValue(params.cookie.value);
        const result = {
            name: params.cookie.name,
            value: deserializedValue,
            domain: params.cookie.domain,
            path: params.cookie.path ?? '/',
            secure: params.cookie.secure ?? false,
            httpOnly: params.cookie.httpOnly ?? false,
            ...(partitionKey.sourceOrigin !== undefined && {
                partitionKey: {
                    hasCrossSiteAncestor: false,
                    // CDP's `partitionKey.topLevelSite` is the BiDi's `partition.sourceOrigin`.
                    topLevelSite: partitionKey.sourceOrigin,
                },
            }),
            ...(params.cookie.expiry !== undefined && {
                expires: params.cookie.expiry,
            }),
            ...(params.cookie.sameSite !== undefined && {
                sameSite: sameSiteBiDiToCdp(params.cookie.sameSite),
            }),
        };
        // Extending with CDP-specific properties with `goog:` prefix.
        if (params.cookie[`goog:url`] !== undefined) {
            result.url = params.cookie[`goog:url`];
        }
        if (params.cookie[`goog:priority`] !== undefined) {
            result.priority = params.cookie[`goog:priority`];
        }
        if (params.cookie[`goog:sameParty`] !== undefined) {
            result.sameParty = params.cookie[`goog:sameParty`];
        }
        if (params.cookie[`goog:sourceScheme`] !== undefined) {
            result.sourceScheme = params.cookie[`goog:sourceScheme`];
        }
        if (params.cookie[`goog:sourcePort`] !== undefined) {
            result.sourcePort = params.cookie[`goog:sourcePort`];
        }
        return result;
    }
    function sameSiteCdpToBiDi(sameSite) {
        switch (sameSite) {
            case 'Strict':
                return "strict" /* Network.SameSite.Strict */;
            case 'None':
                return "none" /* Network.SameSite.None */;
            case 'Lax':
                return "lax" /* Network.SameSite.Lax */;
            default:
                // Defaults to `Lax`:
                // https://web.dev/articles/samesite-cookies-explained#samesitelax_by_default
                return "lax" /* Network.SameSite.Lax */;
        }
    }
    function sameSiteBiDiToCdp(sameSite) {
        switch (sameSite) {
            case "strict" /* Network.SameSite.Strict */:
                return 'Strict';
            case "lax" /* Network.SameSite.Lax */:
                return 'Lax';
            case "none" /* Network.SameSite.None */:
                return 'None';
        }
        throw new InvalidArgumentException(`Unknown 'sameSite' value ${sameSite}`);
    }
    /** Matches the given URLPattern against the given URL. */
    function matchUrlPattern(urlPattern, url) {
        switch (urlPattern.type) {
            case 'string': {
                const pattern = new URLPattern(urlPattern.pattern);
                return new URLPattern({
                    protocol: pattern.protocol,
                    hostname: pattern.hostname,
                    port: pattern.port,
                    pathname: pattern.pathname,
                    search: pattern.search,
                }).test(url);
            }
            case 'pattern':
                return new URLPattern(urlPattern).test(url);
        }
    }
    function bidiBodySizeFromCdpPostDataEntries(entries) {
        let size = 0;
        for (const entry of entries) {
            size += atob(entry.bytes ?? '').length;
        }
        return size;
    }
    function getTiming(timing) {
        if (!timing) {
            return 0;
        }
        if (timing < 0) {
            return 0;
        }
        return timing;
    }

    /**
     * Responsible for handling the `storage` domain.
     */
    class StorageProcessor {
        #browserCdpClient;
        #browsingContextStorage;
        #logger;
        constructor(browserCdpClient, browsingContextStorage, logger) {
            this.#browsingContextStorage = browsingContextStorage;
            this.#browserCdpClient = browserCdpClient;
            this.#logger = logger;
        }
        async deleteCookies(params) {
            const partitionKey = this.#expandStoragePartitionSpec(params.partition);
            let cdpResponse;
            try {
                cdpResponse = await this.#browserCdpClient.sendCommand('Storage.getCookies', {
                    browserContextId: this.#getCdpBrowserContextId(partitionKey),
                });
            }
            catch (err) {
                if (this.#isNoSuchUserContextError(err)) {
                    // If the user context is not found, special error is thrown.
                    throw new NoSuchUserContextException(err.message);
                }
                throw err;
            }
            const cdpCookiesToDelete = cdpResponse.cookies
                .filter(
            // CDP's partition key is the source origin. If the request specifies the
            // `sourceOrigin` partition key, only cookies with the requested source origin
            // are returned.
            (c) => partitionKey.sourceOrigin === undefined ||
                c.partitionKey?.topLevelSite === partitionKey.sourceOrigin)
                .filter((cdpCookie) => {
                const bidiCookie = cdpToBiDiCookie(cdpCookie);
                return this.#matchCookie(bidiCookie, params.filter);
            })
                .map((cookie) => ({
                ...cookie,
                // Set expiry to pass date to delete the cookie.
                expires: 1,
            }));
            await this.#browserCdpClient.sendCommand('Storage.setCookies', {
                cookies: cdpCookiesToDelete,
                browserContextId: this.#getCdpBrowserContextId(partitionKey),
            });
            return {
                partitionKey,
            };
        }
        async getCookies(params) {
            const partitionKey = this.#expandStoragePartitionSpec(params.partition);
            let cdpResponse;
            try {
                cdpResponse = await this.#browserCdpClient.sendCommand('Storage.getCookies', {
                    browserContextId: this.#getCdpBrowserContextId(partitionKey),
                });
            }
            catch (err) {
                if (this.#isNoSuchUserContextError(err)) {
                    // If the user context is not found, special error is thrown.
                    throw new NoSuchUserContextException(err.message);
                }
                throw err;
            }
            const filteredBiDiCookies = cdpResponse.cookies
                .filter(
            // CDP's partition key is the source origin. If the request specifies the
            // `sourceOrigin` partition key, only cookies with the requested source origin
            // are returned.
            (c) => partitionKey.sourceOrigin === undefined ||
                c.partitionKey?.topLevelSite === partitionKey.sourceOrigin)
                .map((c) => cdpToBiDiCookie(c))
                .filter((c) => this.#matchCookie(c, params.filter));
            return {
                cookies: filteredBiDiCookies,
                partitionKey,
            };
        }
        async setCookie(params) {
            const partitionKey = this.#expandStoragePartitionSpec(params.partition);
            const cdpCookie = bidiToCdpCookie(params, partitionKey);
            try {
                await this.#browserCdpClient.sendCommand('Storage.setCookies', {
                    cookies: [cdpCookie],
                    browserContextId: this.#getCdpBrowserContextId(partitionKey),
                });
            }
            catch (err) {
                if (this.#isNoSuchUserContextError(err)) {
                    // If the user context is not found, special error is thrown.
                    throw new NoSuchUserContextException(err.message);
                }
                this.#logger?.(LogType.debugError, err);
                throw new UnableToSetCookieException(err.toString());
            }
            return {
                partitionKey,
            };
        }
        #isNoSuchUserContextError(err) {
            // Heuristic to detect if the user context is not found.
            // See https://source.chromium.org/chromium/chromium/src/+/main:content/browser/devtools/protocol/browser_handler.cc;drc=a56154dd81e4679712422ac6eed2c9581cb51ab0;l=314
            return err.message?.startsWith('Failed to find browser context for id');
        }
        #getCdpBrowserContextId(partitionKey) {
            return partitionKey.userContext === 'default'
                ? undefined
                : partitionKey.userContext;
        }
        #expandStoragePartitionSpecByBrowsingContext(descriptor) {
            const browsingContextId = descriptor.context;
            const browsingContext = this.#browsingContextStorage.getContext(browsingContextId);
            // https://w3c.github.io/webdriver-bidi/#associated-storage-partition.
            // Each browsing context also has an associated storage partition, which is the
            // storage partition it uses to persist data. In Chromium it's a `BrowserContext`
            // which maps to BiDi `UserContext`.
            return {
                userContext: browsingContext.userContext,
            };
        }
        #expandStoragePartitionSpecByStorageKey(descriptor) {
            const unsupportedPartitionKeys = new Map();
            let sourceOrigin = descriptor.sourceOrigin;
            if (sourceOrigin !== undefined) {
                const url = NetworkProcessor.parseUrlString(sourceOrigin);
                if (url.origin === 'null') {
                    // Origin `null` is a special case for local pages.
                    sourceOrigin = url.origin;
                }
                else {
                    // Port is not supported in CDP Cookie's `partitionKey`, so it should be stripped
                    // from the requested source origin.
                    sourceOrigin = `${url.protocol}//${url.hostname}`;
                }
            }
            for (const [key, value] of Object.entries(descriptor)) {
                if (key !== undefined &&
                    value !== undefined &&
                    !['type', 'sourceOrigin', 'userContext'].includes(key)) {
                    unsupportedPartitionKeys.set(key, value);
                }
            }
            if (unsupportedPartitionKeys.size > 0) {
                this.#logger?.(LogType.debugInfo, `Unsupported partition keys: ${JSON.stringify(Object.fromEntries(unsupportedPartitionKeys))}`);
            }
            // Set `userContext` to `default` if not provided, as it's required in Chromium.
            const userContext = descriptor.userContext ?? 'default';
            return {
                userContext,
                ...(sourceOrigin === undefined ? {} : { sourceOrigin }),
            };
        }
        #expandStoragePartitionSpec(partitionSpec) {
            if (partitionSpec === undefined) {
                // `userContext` is required in Chromium.
                return { userContext: 'default' };
            }
            if (partitionSpec.type === 'context') {
                return this.#expandStoragePartitionSpecByBrowsingContext(partitionSpec);
            }
            assert(partitionSpec.type === 'storageKey', 'Unknown partition type');
            // Partition spec is a storage partition.
            // Let partition key be partition spec.
            return this.#expandStoragePartitionSpecByStorageKey(partitionSpec);
        }
        #matchCookie(cookie, filter) {
            if (filter === undefined) {
                return true;
            }
            return ((filter.domain === undefined || filter.domain === cookie.domain) &&
                (filter.name === undefined || filter.name === cookie.name) &&
                // `value` contains fields `type` and `value`.
                (filter.value === undefined ||
                    deserializeByteValue(filter.value) ===
                        deserializeByteValue(cookie.value)) &&
                (filter.path === undefined || filter.path === cookie.path) &&
                (filter.size === undefined || filter.size === cookie.size) &&
                (filter.httpOnly === undefined || filter.httpOnly === cookie.httpOnly) &&
                (filter.secure === undefined || filter.secure === cookie.secure) &&
                (filter.sameSite === undefined || filter.sameSite === cookie.sameSite) &&
                (filter.expiry === undefined || filter.expiry === cookie.expiry));
        }
    }

    /**
     * Copyright 2021 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class OutgoingMessage {
        #message;
        #channel;
        constructor(message, channel = null) {
            this.#message = message;
            this.#channel = channel;
        }
        static createFromPromise(messagePromise, channel) {
            return messagePromise.then((message) => {
                if (message.kind === 'success') {
                    return {
                        kind: 'success',
                        value: new OutgoingMessage(message.value, channel),
                    };
                }
                return message;
            });
        }
        static createResolved(message, channel) {
            return Promise.resolve({
                kind: 'success',
                value: new OutgoingMessage(message, channel),
            });
        }
        get message() {
            return this.#message;
        }
        get channel() {
            return this.#channel;
        }
    }

    /**
     * Copyright 2021 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class CommandProcessor extends EventEmitter {
        // keep-sorted start
        #bluetoothProcessor;
        #browserProcessor;
        #browsingContextProcessor;
        #cdpProcessor;
        #inputProcessor;
        #networkProcessor;
        #permissionsProcessor;
        #scriptProcessor;
        #sessionProcessor;
        #storageProcessor;
        // keep-sorted end
        #parser;
        #logger;
        constructor(cdpConnection, browserCdpClient, eventManager, browsingContextStorage, realmStorage, preloadScriptStorage, networkStorage, bluetoothProcessor, parser = new BidiNoOpParser(), initConnection, logger) {
            super();
            this.#parser = parser;
            this.#logger = logger;
            this.#bluetoothProcessor = bluetoothProcessor;
            // keep-sorted start block=yes
            this.#browserProcessor = new BrowserProcessor(browserCdpClient);
            this.#browsingContextProcessor = new BrowsingContextProcessor(browserCdpClient, browsingContextStorage, eventManager);
            this.#cdpProcessor = new CdpProcessor(browsingContextStorage, realmStorage, cdpConnection, browserCdpClient);
            this.#inputProcessor = new InputProcessor(browsingContextStorage);
            this.#networkProcessor = new NetworkProcessor(browsingContextStorage, networkStorage);
            this.#permissionsProcessor = new PermissionsProcessor(browserCdpClient);
            this.#scriptProcessor = new ScriptProcessor(eventManager, browsingContextStorage, realmStorage, preloadScriptStorage, logger);
            this.#sessionProcessor = new SessionProcessor(eventManager, browserCdpClient, initConnection);
            this.#storageProcessor = new StorageProcessor(browserCdpClient, browsingContextStorage, logger);
            // keep-sorted end
        }
        async #processCommand(command) {
            switch (command.method) {
                case 'session.end':
                    // TODO: Implement.
                    break;
                // Bluetooth domain
                // keep-sorted start block=yes
                case 'bluetooth.handleRequestDevicePrompt':
                    return await this.#bluetoothProcessor.handleRequestDevicePrompt(this.#parser.parseHandleRequestDevicePromptParams(command.params));
                // keep-sorted end
                // Browser domain
                // keep-sorted start block=yes
                case 'browser.close':
                    return this.#browserProcessor.close();
                case 'browser.createUserContext':
                    return await this.#browserProcessor.createUserContext(command.params);
                case 'browser.getClientWindows':
                    throw new UnknownErrorException(`Method ${command.method} is not implemented.`);
                case 'browser.getUserContexts':
                    return await this.#browserProcessor.getUserContexts();
                case 'browser.removeUserContext':
                    return await this.#browserProcessor.removeUserContext(this.#parser.parseRemoveUserContextParams(command.params));
                case 'browser.setClientWindowState':
                    throw new UnknownErrorException(`Method ${command.method} is not implemented.`);
                // keep-sorted end
                // Browsing Context domain
                // keep-sorted start block=yes
                case 'browsingContext.activate':
                    return await this.#browsingContextProcessor.activate(this.#parser.parseActivateParams(command.params));
                case 'browsingContext.captureScreenshot':
                    return await this.#browsingContextProcessor.captureScreenshot(this.#parser.parseCaptureScreenshotParams(command.params));
                case 'browsingContext.close':
                    return await this.#browsingContextProcessor.close(this.#parser.parseCloseParams(command.params));
                case 'browsingContext.create':
                    return await this.#browsingContextProcessor.create(this.#parser.parseCreateParams(command.params));
                case 'browsingContext.getTree':
                    return this.#browsingContextProcessor.getTree(this.#parser.parseGetTreeParams(command.params));
                case 'browsingContext.handleUserPrompt':
                    return await this.#browsingContextProcessor.handleUserPrompt(this.#parser.parseHandleUserPromptParams(command.params));
                case 'browsingContext.locateNodes':
                    return await this.#browsingContextProcessor.locateNodes(this.#parser.parseLocateNodesParams(command.params));
                case 'browsingContext.navigate':
                    return await this.#browsingContextProcessor.navigate(this.#parser.parseNavigateParams(command.params));
                case 'browsingContext.print':
                    return await this.#browsingContextProcessor.print(this.#parser.parsePrintParams(command.params));
                case 'browsingContext.reload':
                    return await this.#browsingContextProcessor.reload(this.#parser.parseReloadParams(command.params));
                case 'browsingContext.setViewport':
                    return await this.#browsingContextProcessor.setViewport(this.#parser.parseSetViewportParams(command.params));
                case 'browsingContext.traverseHistory':
                    return await this.#browsingContextProcessor.traverseHistory(this.#parser.parseTraverseHistoryParams(command.params));
                // keep-sorted end
                // CDP domain
                // keep-sorted start block=yes
                case 'cdp.getSession':
                    return this.#cdpProcessor.getSession(this.#parser.parseGetSessionParams(command.params));
                case 'cdp.resolveRealm':
                    return this.#cdpProcessor.resolveRealm(this.#parser.parseResolveRealmParams(command.params));
                case 'cdp.sendCommand':
                    return await this.#cdpProcessor.sendCommand(this.#parser.parseSendCommandParams(command.params));
                // keep-sorted end
                // Input domain
                // keep-sorted start block=yes
                case 'input.performActions':
                    return await this.#inputProcessor.performActions(this.#parser.parsePerformActionsParams(command.params));
                case 'input.releaseActions':
                    return await this.#inputProcessor.releaseActions(this.#parser.parseReleaseActionsParams(command.params));
                case 'input.setFiles':
                    return await this.#inputProcessor.setFiles(this.#parser.parseSetFilesParams(command.params));
                // keep-sorted end
                // Network domain
                // keep-sorted start block=yes
                case 'network.addIntercept':
                    return await this.#networkProcessor.addIntercept(this.#parser.parseAddInterceptParams(command.params));
                case 'network.continueRequest':
                    return await this.#networkProcessor.continueRequest(this.#parser.parseContinueRequestParams(command.params));
                case 'network.continueResponse':
                    return await this.#networkProcessor.continueResponse(this.#parser.parseContinueResponseParams(command.params));
                case 'network.continueWithAuth':
                    return await this.#networkProcessor.continueWithAuth(this.#parser.parseContinueWithAuthParams(command.params));
                case 'network.failRequest':
                    return await this.#networkProcessor.failRequest(this.#parser.parseFailRequestParams(command.params));
                case 'network.provideResponse':
                    return await this.#networkProcessor.provideResponse(this.#parser.parseProvideResponseParams(command.params));
                case 'network.removeIntercept':
                    return await this.#networkProcessor.removeIntercept(this.#parser.parseRemoveInterceptParams(command.params));
                case 'network.setCacheBehavior':
                    return await this.#networkProcessor.setCacheBehavior(this.#parser.parseSetCacheBehavior(command.params));
                // keep-sorted end
                // Permissions domain
                // keep-sorted start block=yes
                case 'permissions.setPermission':
                    return await this.#permissionsProcessor.setPermissions(this.#parser.parseSetPermissionsParams(command.params));
                // keep-sorted end
                // Script domain
                // keep-sorted start block=yes
                case 'script.addPreloadScript':
                    return await this.#scriptProcessor.addPreloadScript(this.#parser.parseAddPreloadScriptParams(command.params));
                case 'script.callFunction':
                    return await this.#scriptProcessor.callFunction(this.#parser.parseCallFunctionParams(this.#processTargetParams(command.params)));
                case 'script.disown':
                    return await this.#scriptProcessor.disown(this.#parser.parseDisownParams(this.#processTargetParams(command.params)));
                case 'script.evaluate':
                    return await this.#scriptProcessor.evaluate(this.#parser.parseEvaluateParams(this.#processTargetParams(command.params)));
                case 'script.getRealms':
                    return this.#scriptProcessor.getRealms(this.#parser.parseGetRealmsParams(command.params));
                case 'script.removePreloadScript':
                    return await this.#scriptProcessor.removePreloadScript(this.#parser.parseRemovePreloadScriptParams(command.params));
                // keep-sorted end
                // Session domain
                // keep-sorted start block=yes
                case 'session.new':
                    return await this.#sessionProcessor.new(command.params);
                case 'session.status':
                    return this.#sessionProcessor.status();
                case 'session.subscribe':
                    return await this.#sessionProcessor.subscribe(this.#parser.parseSubscribeParams(command.params), command.channel);
                case 'session.unsubscribe':
                    return await this.#sessionProcessor.unsubscribe(this.#parser.parseSubscribeParams(command.params), command.channel);
                // keep-sorted end
                // Storage domain
                // keep-sorted start block=yes
                case 'storage.deleteCookies':
                    return await this.#storageProcessor.deleteCookies(this.#parser.parseDeleteCookiesParams(command.params));
                case 'storage.getCookies':
                    return await this.#storageProcessor.getCookies(this.#parser.parseGetCookiesParams(command.params));
                case 'storage.setCookie':
                    return await this.#storageProcessor.setCookie(this.#parser.parseSetCookieParams(command.params));
                // keep-sorted end
            }
            // Intentionally kept outside the switch statement to ensure that
            // ESLint @typescript-eslint/switch-exhaustiveness-check triggers if a new
            // command is added.
            throw new UnknownCommandException(`Unknown command '${command.method}'.`);
        }
        // Workaround for as zod.union always take the first schema
        // https://github.com/w3c/webdriver-bidi/issues/635
        #processTargetParams(params) {
            if (typeof params === 'object' &&
                params &&
                'target' in params &&
                typeof params.target === 'object' &&
                params.target &&
                'context' in params.target) {
                delete params.target['realm'];
            }
            return params;
        }
        async processCommand(command) {
            try {
                const result = await this.#processCommand(command);
                const response = {
                    type: 'success',
                    id: command.id,
                    result,
                };
                this.emit("response" /* CommandProcessorEvents.Response */, {
                    message: OutgoingMessage.createResolved(response, command.channel),
                    event: command.method,
                });
            }
            catch (e) {
                if (e instanceof Exception) {
                    this.emit("response" /* CommandProcessorEvents.Response */, {
                        message: OutgoingMessage.createResolved(e.toErrorResponse(command.id), command.channel),
                        event: command.method,
                    });
                }
                else {
                    const error = e;
                    this.#logger?.(LogType.bidi, error);
                    this.emit("response" /* CommandProcessorEvents.Response */, {
                        message: OutgoingMessage.createResolved(new UnknownErrorException(error.message, error.stack).toErrorResponse(command.id), command.channel),
                        event: command.method,
                    });
                }
            }
        }
    }

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class BluetoothProcessor {
        #eventManager;
        #browsingContextStorage;
        constructor(eventManager, browsingContextStorage) {
            this.#eventManager = eventManager;
            this.#browsingContextStorage = browsingContextStorage;
        }
        onCdpTargetCreated(cdpTarget) {
            cdpTarget.cdpClient.on('DeviceAccess.deviceRequestPrompted', (event) => {
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: 'bluetooth.requestDevicePromptUpdated',
                    params: {
                        context: cdpTarget.id,
                        prompt: event.id,
                        devices: event.devices,
                    },
                }, cdpTarget.id);
            });
        }
        async handleRequestDevicePrompt(params) {
            const context = this.#browsingContextStorage.getContext(params.context);
            if (params.accept) {
                await context.cdpTarget.cdpClient.sendCommand('DeviceAccess.selectPrompt', {
                    id: params.prompt,
                    deviceId: params.device,
                });
            }
            else {
                await context.cdpTarget.cdpClient.sendCommand('DeviceAccess.cancelPrompt', {
                    id: params.prompt,
                });
            }
            return {};
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class Deferred {
        #isFinished = false;
        #promise;
        #result;
        #resolve;
        #reject;
        get isFinished() {
            return this.#isFinished;
        }
        get result() {
            if (!this.#isFinished) {
                throw new Error('Deferred is not finished yet');
            }
            return this.#result;
        }
        constructor() {
            this.#promise = new Promise((resolve, reject) => {
                this.#resolve = resolve;
                this.#reject = reject;
            });
            // Needed to avoid `Uncaught (in promise)`. The promises returned by `then`
            // and `catch` will be rejected anyway.
            this.#promise.catch((_error) => {
                // Intentionally empty.
            });
        }
        then(onFulfilled, onRejected) {
            return this.#promise.then(onFulfilled, onRejected);
        }
        catch(onRejected) {
            return this.#promise.catch(onRejected);
        }
        resolve(value) {
            this.#result = value;
            if (!this.#isFinished) {
                this.#isFinished = true;
                this.#resolve(value);
            }
        }
        reject(reason) {
            if (!this.#isFinished) {
                this.#isFinished = true;
                this.#reject(reason);
            }
        }
        finally(onFinally) {
            return this.#promise.finally(onFinally);
        }
        [Symbol.toStringTag] = 'Promise';
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** @return Given an input in cm, convert it to inches. */
    function inchesFromCm(cm) {
        return cm / 2.54;
    }

    class Realm {
        #cdpClient;
        #eventManager;
        #executionContextId;
        #logger;
        #origin;
        #realmId;
        #realmStorage;
        constructor(cdpClient, eventManager, executionContextId, logger, origin, realmId, realmStorage) {
            this.#cdpClient = cdpClient;
            this.#eventManager = eventManager;
            this.#executionContextId = executionContextId;
            this.#logger = logger;
            this.#origin = origin;
            this.#realmId = realmId;
            this.#realmStorage = realmStorage;
            this.#realmStorage.addRealm(this);
        }
        cdpToBidiValue(cdpValue, resultOwnership) {
            const bidiValue = this.serializeForBiDi(cdpValue.result.deepSerializedValue, new Map());
            if (cdpValue.result.objectId) {
                const objectId = cdpValue.result.objectId;
                if (resultOwnership === "root" /* Script.ResultOwnership.Root */) {
                    // Extend BiDi value with `handle` based on required `resultOwnership`
                    // and  CDP response but not on the actual BiDi type.
                    bidiValue.handle = objectId;
                    // Remember all the handles sent to client.
                    this.#realmStorage.knownHandlesToRealmMap.set(objectId, this.realmId);
                }
                else {
                    // No need to await for the object to be released.
                    void this.#releaseObject(objectId).catch((error) => this.#logger?.(LogType.debugError, error));
                }
            }
            return bidiValue;
        }
        /**
         * Relies on the CDP to implement proper BiDi serialization, except:
         * * CDP integer property `backendNodeId` is replaced with `sharedId` of
         * `{documentId}_element_{backendNodeId}`;
         * * CDP integer property `weakLocalObjectReference` is replaced with UUID `internalId`
         * using unique-per serialization `internalIdMap`.
         * * CDP type `platformobject` is replaced with `object`.
         * @param deepSerializedValue - CDP value to be converted to BiDi.
         * @param internalIdMap - Map from CDP integer `weakLocalObjectReference` to BiDi UUID
         * `internalId`.
         */
        serializeForBiDi(deepSerializedValue, internalIdMap) {
            if (Object.hasOwn(deepSerializedValue, 'weakLocalObjectReference')) {
                const weakLocalObjectReference = deepSerializedValue.weakLocalObjectReference;
                if (!internalIdMap.has(weakLocalObjectReference)) {
                    internalIdMap.set(weakLocalObjectReference, uuidv4());
                }
                deepSerializedValue.internalId = internalIdMap.get(weakLocalObjectReference);
                delete deepSerializedValue['weakLocalObjectReference'];
            }
            if (deepSerializedValue.type === 'node' &&
                Object.hasOwn(deepSerializedValue?.value, 'frameId')) {
                // `frameId` is not needed in BiDi as it is not yet specified.
                delete deepSerializedValue.value['frameId'];
            }
            // Platform object is a special case. It should have only `{type: object}`
            // without `value` field.
            if (deepSerializedValue.type === 'platformobject') {
                return { type: 'object' };
            }
            const bidiValue = deepSerializedValue.value;
            if (bidiValue === undefined) {
                return deepSerializedValue;
            }
            // Recursively update the nested values.
            if (['array', 'set', 'htmlcollection', 'nodelist'].includes(deepSerializedValue.type)) {
                for (const i in bidiValue) {
                    bidiValue[i] = this.serializeForBiDi(bidiValue[i], internalIdMap);
                }
            }
            if (['object', 'map'].includes(deepSerializedValue.type)) {
                for (const i in bidiValue) {
                    bidiValue[i] = [
                        this.serializeForBiDi(bidiValue[i][0], internalIdMap),
                        this.serializeForBiDi(bidiValue[i][1], internalIdMap),
                    ];
                }
            }
            return deepSerializedValue;
        }
        get realmId() {
            return this.#realmId;
        }
        get executionContextId() {
            return this.#executionContextId;
        }
        get origin() {
            return this.#origin;
        }
        get source() {
            return {
                realm: this.realmId,
            };
        }
        get cdpClient() {
            return this.#cdpClient;
        }
        get baseInfo() {
            return {
                realm: this.realmId,
                origin: this.origin,
            };
        }
        async evaluate(expression, awaitPromise, resultOwnership = "none" /* Script.ResultOwnership.None */, serializationOptions = {}, userActivation = false, includeCommandLineApi = false) {
            const cdpEvaluateResult = await this.cdpClient.sendCommand('Runtime.evaluate', {
                contextId: this.executionContextId,
                expression,
                awaitPromise,
                serializationOptions: Realm.#getSerializationOptions("deep" /* Protocol.Runtime.SerializationOptionsSerialization.Deep */, serializationOptions),
                userGesture: userActivation,
                includeCommandLineAPI: includeCommandLineApi,
            });
            if (cdpEvaluateResult.exceptionDetails) {
                return await this.#getExceptionResult(cdpEvaluateResult.exceptionDetails, 0, resultOwnership);
            }
            return {
                realm: this.realmId,
                result: this.cdpToBidiValue(cdpEvaluateResult, resultOwnership),
                type: 'success',
            };
        }
        #registerEvent(event) {
            if (this.associatedBrowsingContexts.length === 0) {
                this.#eventManager.registerEvent(event, null);
            }
            else {
                for (const browsingContext of this.associatedBrowsingContexts) {
                    this.#eventManager.registerEvent(event, browsingContext.id);
                }
            }
        }
        initialize() {
            this.#registerEvent({
                type: 'event',
                method: Script$2.EventNames.RealmCreated,
                params: this.realmInfo,
            });
        }
        /**
         * Serializes a given CDP object into BiDi, keeping references in the
         * target's `globalThis`.
         */
        async serializeCdpObject(cdpRemoteObject, resultOwnership) {
            // TODO: if the object is a primitive, return it directly without CDP roundtrip.
            const argument = Realm.#cdpRemoteObjectToCallArgument(cdpRemoteObject);
            const cdpValue = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                functionDeclaration: String((remoteObject) => remoteObject),
                awaitPromise: false,
                arguments: [argument],
                serializationOptions: {
                    serialization: "deep" /* Protocol.Runtime.SerializationOptionsSerialization.Deep */,
                },
                executionContextId: this.executionContextId,
            });
            return this.cdpToBidiValue(cdpValue, resultOwnership);
        }
        static #cdpRemoteObjectToCallArgument(cdpRemoteObject) {
            if (cdpRemoteObject.objectId !== undefined) {
                return { objectId: cdpRemoteObject.objectId };
            }
            if (cdpRemoteObject.unserializableValue !== undefined) {
                return { unserializableValue: cdpRemoteObject.unserializableValue };
            }
            return { value: cdpRemoteObject.value };
        }
        /**
         * Gets the string representation of an object. This is equivalent to
         * calling `toString()` on the object value.
         */
        async stringifyObject(cdpRemoteObject) {
            const { result } = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                functionDeclaration: String((remoteObject) => String(remoteObject)),
                awaitPromise: false,
                arguments: [cdpRemoteObject],
                returnByValue: true,
                executionContextId: this.executionContextId,
            });
            return result.value;
        }
        async #flattenKeyValuePairs(mappingLocalValue) {
            const keyValueArray = await Promise.all(mappingLocalValue.map(async ([key, value]) => {
                let keyArg;
                if (typeof key === 'string') {
                    // Key is a string.
                    keyArg = { value: key };
                }
                else {
                    // Key is a serialized value.
                    keyArg = await this.deserializeForCdp(key);
                }
                const valueArg = await this.deserializeForCdp(value);
                return [keyArg, valueArg];
            }));
            return keyValueArray.flat();
        }
        async #flattenValueList(listLocalValue) {
            return await Promise.all(listLocalValue.map((localValue) => this.deserializeForCdp(localValue)));
        }
        async #serializeCdpExceptionDetails(cdpExceptionDetails, lineOffset, resultOwnership) {
            const callFrames = cdpExceptionDetails.stackTrace?.callFrames.map((frame) => ({
                url: frame.url,
                functionName: frame.functionName,
                lineNumber: frame.lineNumber - lineOffset,
                columnNumber: frame.columnNumber,
            })) ?? [];
            // Exception should always be there.
            const exception = cdpExceptionDetails.exception;
            return {
                exception: await this.serializeCdpObject(exception, resultOwnership),
                columnNumber: cdpExceptionDetails.columnNumber,
                lineNumber: cdpExceptionDetails.lineNumber - lineOffset,
                stackTrace: {
                    callFrames,
                },
                text: (await this.stringifyObject(exception)) || cdpExceptionDetails.text,
            };
        }
        async callFunction(functionDeclaration, awaitPromise, thisLocalValue = {
            type: 'undefined',
        }, argumentsLocalValues = [], resultOwnership = "none" /* Script.ResultOwnership.None */, serializationOptions = {}, userActivation = false) {
            const callFunctionAndSerializeScript = `(...args) => {
      function callFunction(f, args) {
        const deserializedThis = args.shift();
        const deserializedArgs = args;
        return f.apply(deserializedThis, deserializedArgs);
      }
      return callFunction((
        ${functionDeclaration}
      ), args);
    }`;
            const thisAndArgumentsList = [
                await this.deserializeForCdp(thisLocalValue),
                ...(await Promise.all(argumentsLocalValues.map(async (argumentLocalValue) => await this.deserializeForCdp(argumentLocalValue)))),
            ];
            let cdpCallFunctionResult;
            try {
                cdpCallFunctionResult = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                    functionDeclaration: callFunctionAndSerializeScript,
                    awaitPromise,
                    arguments: thisAndArgumentsList,
                    serializationOptions: Realm.#getSerializationOptions("deep" /* Protocol.Runtime.SerializationOptionsSerialization.Deep */, serializationOptions),
                    executionContextId: this.executionContextId,
                    userGesture: userActivation,
                });
            }
            catch (error) {
                // Heuristic to determine if the problem is in the argument.
                // The check can be done on the `deserialization` step, but this approach
                // helps to save round-trips.
                if (error.code === -32000 /* CdpErrorConstants.GENERIC_ERROR */ &&
                    [
                        'Could not find object with given id',
                        'Argument should belong to the same JavaScript world as target object',
                        'Invalid remote object id',
                    ].includes(error.message)) {
                    throw new NoSuchHandleException('Handle was not found.');
                }
                throw error;
            }
            if (cdpCallFunctionResult.exceptionDetails) {
                return await this.#getExceptionResult(cdpCallFunctionResult.exceptionDetails, 1, resultOwnership);
            }
            return {
                type: 'success',
                result: this.cdpToBidiValue(cdpCallFunctionResult, resultOwnership),
                realm: this.realmId,
            };
        }
        async deserializeForCdp(localValue) {
            if ('handle' in localValue && localValue.handle) {
                return { objectId: localValue.handle };
                // We tried to find a handle value but failed
                // This allows us to have exhaustive switch on `localValue.type`
            }
            else if ('handle' in localValue || 'sharedId' in localValue) {
                throw new NoSuchHandleException('Handle was not found.');
            }
            switch (localValue.type) {
                case 'undefined':
                    return { unserializableValue: 'undefined' };
                case 'null':
                    return { unserializableValue: 'null' };
                case 'string':
                    return { value: localValue.value };
                case 'number':
                    if (localValue.value === 'NaN') {
                        return { unserializableValue: 'NaN' };
                    }
                    else if (localValue.value === '-0') {
                        return { unserializableValue: '-0' };
                    }
                    else if (localValue.value === 'Infinity') {
                        return { unserializableValue: 'Infinity' };
                    }
                    else if (localValue.value === '-Infinity') {
                        return { unserializableValue: '-Infinity' };
                    }
                    return {
                        value: localValue.value,
                    };
                case 'boolean':
                    return { value: Boolean(localValue.value) };
                case 'bigint':
                    return {
                        unserializableValue: `BigInt(${JSON.stringify(localValue.value)})`,
                    };
                case 'date':
                    return {
                        unserializableValue: `new Date(Date.parse(${JSON.stringify(localValue.value)}))`,
                    };
                case 'regexp':
                    return {
                        unserializableValue: `new RegExp(${JSON.stringify(localValue.value.pattern)}, ${JSON.stringify(localValue.value.flags)})`,
                    };
                case 'map': {
                    // TODO: If none of the nested keys and values has a remote
                    // reference, serialize to `unserializableValue` without CDP roundtrip.
                    const keyValueArray = await this.#flattenKeyValuePairs(localValue.value);
                    const { result } = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                        functionDeclaration: String((...args) => {
                            const result = new Map();
                            for (let i = 0; i < args.length; i += 2) {
                                result.set(args[i], args[i + 1]);
                            }
                            return result;
                        }),
                        awaitPromise: false,
                        arguments: keyValueArray,
                        returnByValue: false,
                        executionContextId: this.executionContextId,
                    });
                    // TODO(#375): Release `result.objectId` after using.
                    return { objectId: result.objectId };
                }
                case 'object': {
                    // TODO: If none of the nested keys and values has a remote
                    // reference, serialize to `unserializableValue` without CDP roundtrip.
                    const keyValueArray = await this.#flattenKeyValuePairs(localValue.value);
                    const { result } = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                        functionDeclaration: String((...args) => {
                            const result = {};
                            for (let i = 0; i < args.length; i += 2) {
                                // Key should be either `string`, `number`, or `symbol`.
                                const key = args[i];
                                result[key] = args[i + 1];
                            }
                            return result;
                        }),
                        awaitPromise: false,
                        arguments: keyValueArray,
                        returnByValue: false,
                        executionContextId: this.executionContextId,
                    });
                    // TODO(#375): Release `result.objectId` after using.
                    return { objectId: result.objectId };
                }
                case 'array': {
                    // TODO: If none of the nested items has a remote reference,
                    // serialize to `unserializableValue` without CDP roundtrip.
                    const args = await this.#flattenValueList(localValue.value);
                    const { result } = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                        functionDeclaration: String((...args) => args),
                        awaitPromise: false,
                        arguments: args,
                        returnByValue: false,
                        executionContextId: this.executionContextId,
                    });
                    // TODO(#375): Release `result.objectId` after using.
                    return { objectId: result.objectId };
                }
                case 'set': {
                    // TODO: if none of the nested items has a remote reference,
                    // serialize to `unserializableValue` without CDP roundtrip.
                    const args = await this.#flattenValueList(localValue.value);
                    const { result } = await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
                        functionDeclaration: String((...args) => new Set(args)),
                        awaitPromise: false,
                        arguments: args,
                        returnByValue: false,
                        executionContextId: this.executionContextId,
                    });
                    // TODO(#375): Release `result.objectId` after using.
                    return { objectId: result.objectId };
                }
                case 'channel': {
                    const channelProxy = new ChannelProxy(localValue.value, this.#logger);
                    const channelProxySendMessageHandle = await channelProxy.init(this, this.#eventManager);
                    return { objectId: channelProxySendMessageHandle };
                }
                // TODO(#375): Dispose of nested objects.
            }
            // Intentionally outside to handle unknown types
            throw new Error(`Value ${JSON.stringify(localValue)} is not deserializable.`);
        }
        async #getExceptionResult(exceptionDetails, lineOffset, resultOwnership) {
            return {
                exceptionDetails: await this.#serializeCdpExceptionDetails(exceptionDetails, lineOffset, resultOwnership),
                realm: this.realmId,
                type: 'exception',
            };
        }
        static #getSerializationOptions(serialization, serializationOptions) {
            return {
                serialization,
                additionalParameters: Realm.#getAdditionalSerializationParameters(serializationOptions),
                ...Realm.#getMaxObjectDepth(serializationOptions),
            };
        }
        static #getAdditionalSerializationParameters(serializationOptions) {
            const additionalParameters = {};
            if (serializationOptions.maxDomDepth !== undefined) {
                additionalParameters['maxNodeDepth'] =
                    serializationOptions.maxDomDepth === null
                        ? 1000
                        : serializationOptions.maxDomDepth;
            }
            if (serializationOptions.includeShadowTree !== undefined) {
                additionalParameters['includeShadowTree'] =
                    serializationOptions.includeShadowTree;
            }
            return additionalParameters;
        }
        static #getMaxObjectDepth(serializationOptions) {
            return serializationOptions.maxObjectDepth === undefined ||
                serializationOptions.maxObjectDepth === null
                ? {}
                : { maxDepth: serializationOptions.maxObjectDepth };
        }
        async #releaseObject(handle) {
            try {
                await this.cdpClient.sendCommand('Runtime.releaseObject', {
                    objectId: handle,
                });
            }
            catch (error) {
                // Heuristic to determine if the problem is in the unknown handler.
                // Ignore the error if so.
                if (!(error.code === -32000 /* CdpErrorConstants.GENERIC_ERROR */ &&
                    error.message === 'Invalid remote object id')) {
                    throw error;
                }
            }
        }
        async disown(handle) {
            // Disowning an object from different realm does nothing.
            if (this.#realmStorage.knownHandlesToRealmMap.get(handle) !== this.realmId) {
                return;
            }
            await this.#releaseObject(handle);
            this.#realmStorage.knownHandlesToRealmMap.delete(handle);
        }
        dispose() {
            this.#registerEvent({
                type: 'event',
                method: Script$2.EventNames.RealmDestroyed,
                params: {
                    realm: this.realmId,
                },
            });
        }
    }

    /*
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    const SHARED_ID_DIVIDER = '_element_';
    function getSharedId(frameId, documentId, backendNodeId) {
        return `f.${frameId}.d.${documentId}.e.${backendNodeId}`;
    }
    function parseLegacySharedId(sharedId) {
        const match = sharedId.match(new RegExp(`(.*)${SHARED_ID_DIVIDER}(.*)`));
        if (!match) {
            // SharedId is incorrectly formatted.
            return null;
        }
        const documentId = match[1];
        const elementId = match[2];
        if (documentId === undefined || elementId === undefined) {
            return null;
        }
        const backendNodeId = parseInt(elementId ?? '');
        if (isNaN(backendNodeId)) {
            return null;
        }
        return {
            documentId,
            backendNodeId,
        };
    }
    function parseSharedId(sharedId) {
        // TODO: remove legacy check once ChromeDriver provides sharedId in the new format.
        const legacyFormattedSharedId = parseLegacySharedId(sharedId);
        if (legacyFormattedSharedId !== null) {
            return { ...legacyFormattedSharedId, frameId: undefined };
        }
        const match = sharedId.match(/f\.(.*)\.d\.(.*)\.e\.([0-9]*)/);
        if (!match) {
            // SharedId is incorrectly formatted.
            return null;
        }
        const frameId = match[1];
        const documentId = match[2];
        const elementId = match[3];
        if (frameId === undefined ||
            documentId === undefined ||
            elementId === undefined) {
            return null;
        }
        const backendNodeId = parseInt(elementId ?? '');
        if (isNaN(backendNodeId)) {
            return null;
        }
        return {
            frameId,
            documentId,
            backendNodeId,
        };
    }

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class WindowRealm extends Realm {
        #browsingContextId;
        #browsingContextStorage;
        sandbox;
        constructor(browsingContextId, browsingContextStorage, cdpClient, eventManager, executionContextId, logger, origin, realmId, realmStorage, sandbox) {
            super(cdpClient, eventManager, executionContextId, logger, origin, realmId, realmStorage);
            this.#browsingContextId = browsingContextId;
            this.#browsingContextStorage = browsingContextStorage;
            this.sandbox = sandbox;
            this.initialize();
        }
        #getBrowsingContextId(navigableId) {
            const maybeBrowsingContext = this.#browsingContextStorage
                .getAllContexts()
                .find((context) => context.navigableId === navigableId);
            return maybeBrowsingContext?.id ?? 'UNKNOWN';
        }
        get browsingContext() {
            return this.#browsingContextStorage.getContext(this.#browsingContextId);
        }
        get associatedBrowsingContexts() {
            return [this.browsingContext];
        }
        get realmType() {
            return 'window';
        }
        get realmInfo() {
            return {
                ...this.baseInfo,
                type: this.realmType,
                context: this.#browsingContextId,
                sandbox: this.sandbox,
            };
        }
        get source() {
            return {
                realm: this.realmId,
                context: this.browsingContext.id,
            };
        }
        serializeForBiDi(deepSerializedValue, internalIdMap) {
            const bidiValue = deepSerializedValue.value;
            if (deepSerializedValue.type === 'node' && bidiValue !== undefined) {
                if (Object.hasOwn(bidiValue, 'backendNodeId')) {
                    let navigableId = this.browsingContext.navigableId ?? 'UNKNOWN';
                    if (Object.hasOwn(bidiValue, 'loaderId')) {
                        // `loaderId` should be always there after ~2024-03-05, when
                        // https://crrev.com/c/5116240 reaches stable.
                        // TODO: remove the check after the date.
                        navigableId = bidiValue.loaderId;
                        delete bidiValue['loaderId'];
                    }
                    deepSerializedValue.sharedId =
                        getSharedId(this.#getBrowsingContextId(navigableId), navigableId, bidiValue.backendNodeId);
                    delete bidiValue['backendNodeId'];
                }
                if (Object.hasOwn(bidiValue, 'children')) {
                    for (const i in bidiValue.children) {
                        bidiValue.children[i] = this.serializeForBiDi(bidiValue.children[i], internalIdMap);
                    }
                }
                if (Object.hasOwn(bidiValue, 'shadowRoot') &&
                    bidiValue.shadowRoot !== null) {
                    bidiValue.shadowRoot = this.serializeForBiDi(bidiValue.shadowRoot, internalIdMap);
                }
                // `namespaceURI` can be is either `null` or non-empty string.
                if (bidiValue.namespaceURI === '') {
                    bidiValue.namespaceURI = null;
                }
            }
            return super.serializeForBiDi(deepSerializedValue, internalIdMap);
        }
        async deserializeForCdp(localValue) {
            if ('sharedId' in localValue && localValue.sharedId) {
                const parsedSharedId = parseSharedId(localValue.sharedId);
                if (parsedSharedId === null) {
                    throw new NoSuchNodeException(`SharedId "${localValue.sharedId}" was not found.`);
                }
                const { documentId, backendNodeId } = parsedSharedId;
                // TODO: add proper validation if the element is accessible from the current realm.
                if (this.browsingContext.navigableId !== documentId) {
                    throw new NoSuchNodeException(`SharedId "${localValue.sharedId}" belongs to different document. Current document is ${this.browsingContext.navigableId}.`);
                }
                try {
                    const { object } = await this.cdpClient.sendCommand('DOM.resolveNode', {
                        backendNodeId,
                        executionContextId: this.executionContextId,
                    });
                    // TODO(#375): Release `obj.object.objectId` after using.
                    return { objectId: object.objectId };
                }
                catch (error) {
                    // Heuristic to detect "no such node" exception. Based on the  specific
                    // CDP implementation.
                    if (error.code === -32000 /* CdpErrorConstants.GENERIC_ERROR */ &&
                        error.message === 'No node with given id found') {
                        throw new NoSuchNodeException(`SharedId "${localValue.sharedId}" was not found.`);
                    }
                    throw new UnknownErrorException(error.message, error.stack);
                }
            }
            return await super.deserializeForCdp(localValue);
        }
        async evaluate(expression, awaitPromise, resultOwnership, serializationOptions, userActivation, includeCommandLineApi) {
            await this.#browsingContextStorage
                .getContext(this.#browsingContextId)
                .targetUnblockedOrThrow();
            return await super.evaluate(expression, awaitPromise, resultOwnership, serializationOptions, userActivation, includeCommandLineApi);
        }
        async callFunction(functionDeclaration, awaitPromise, thisLocalValue, argumentsLocalValues, resultOwnership, serializationOptions, userActivation) {
            await this.#browsingContextStorage
                .getContext(this.#browsingContextId)
                .targetUnblockedOrThrow();
            return await super.callFunction(functionDeclaration, awaitPromise, thisLocalValue, argumentsLocalValues, resultOwnership, serializationOptions, userActivation);
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    var _a$5;
    class BrowsingContextImpl {
        static LOGGER_PREFIX = `${LogType.debug}:browsingContext`;
        /** The ID of this browsing context. */
        #id;
        userContext;
        /**
         * The ID of the parent browsing context.
         * If null, this is a top-level context.
         */
        #parentId = null;
        /** Direct children browsing contexts. */
        #children = new Set();
        #browsingContextStorage;
        #lifecycle = {
            DOMContentLoaded: new Deferred(),
            load: new Deferred(),
        };
        #navigation = {
            withinDocument: new Deferred(),
        };
        #url;
        #eventManager;
        #realmStorage;
        #loaderId;
        #cdpTarget;
        // The deferred will be resolved when the default realm is created.
        #defaultRealmDeferred = new Deferred();
        #logger;
        // Keeps track of the previously set viewport.
        #previousViewport = { width: 0, height: 0 };
        // The URL of the navigation that is currently in progress. A workaround of the CDP
        // lacking URL for the pending navigation events, e.g. `Page.frameStartedLoading`.
        // Set on `Page.navigate`, `Page.reload` commands, on `Page.frameRequestedNavigation` or
        // on a deprecated `Page.frameScheduledNavigation` event. The latest is required as the
        // `Page.frameRequestedNavigation` event is not emitted for same-document navigations.
        #pendingNavigationUrl;
        // Navigation ID is required, as CDP `loaderId` cannot be mapped 1:1 to all the
        // navigations (e.g. same document navigations). Updated after each navigation,
        // including same-document ones.
        #navigationId = uuidv4();
        // When a new navigation is started via `BrowsingContext.navigate` with `wait` set to
        // `None`, the command result should have `navigation` value, but mapper does not have
        // it yet. This value will be set to `navigationId` after next .
        #pendingNavigationId;
        // Set if there is a pending navigation initiated by `BrowsingContext.navigate` command.
        // The promise is resolved when the navigation is finished or rejected when canceled.
        #pendingCommandNavigation;
        #originalOpener;
        // Set when the user prompt is opened. Required to provide the type in closing event.
        #lastUserPromptType;
        #unhandledPromptBehavior;
        constructor(id, parentId, userContext, cdpTarget, eventManager, browsingContextStorage, realmStorage, url, originalOpener, unhandledPromptBehavior, logger) {
            this.#cdpTarget = cdpTarget;
            this.#id = id;
            this.#parentId = parentId;
            this.userContext = userContext;
            this.#eventManager = eventManager;
            this.#browsingContextStorage = browsingContextStorage;
            this.#realmStorage = realmStorage;
            this.#unhandledPromptBehavior = unhandledPromptBehavior;
            this.#logger = logger;
            this.#url = url;
            this.#originalOpener = originalOpener;
        }
        static create(id, parentId, userContext, cdpTarget, eventManager, browsingContextStorage, realmStorage, url, originalOpener, unhandledPromptBehavior, logger) {
            const context = new _a$5(id, parentId, userContext, cdpTarget, eventManager, browsingContextStorage, realmStorage, url, originalOpener, unhandledPromptBehavior, logger);
            context.#initListeners();
            browsingContextStorage.addContext(context);
            if (!context.isTopLevelContext()) {
                context.parent.addChild(context.id);
            }
            // Hold on the `contextCreated` event until the target is unblocked. This is required,
            // as the parent of the context can be set later in case of reconnecting to an
            // existing browser instance + OOPiF.
            eventManager.registerPromiseEvent(context.targetUnblockedOrThrow().then(() => {
                return {
                    kind: 'success',
                    value: {
                        type: 'event',
                        method: BrowsingContext$2.EventNames.ContextCreated,
                        params: context.serializeToBidiValue(),
                    },
                };
            }, (error) => {
                return {
                    kind: 'error',
                    error,
                };
            }), context.id, BrowsingContext$2.EventNames.ContextCreated);
            return context;
        }
        static getTimestamp() {
            // `timestamp` from the event is MonotonicTime, not real time, so
            // the best Mapper can do is to set the timestamp to the epoch time
            // of the event arrived.
            // https://chromedevtools.github.io/devtools-protocol/tot/Network/#type-MonotonicTime
            return new Date().getTime();
        }
        /**
         * @see https://html.spec.whatwg.org/multipage/document-sequences.html#navigable
         */
        get navigableId() {
            return this.#loaderId;
        }
        get navigationId() {
            return this.#navigationId;
        }
        dispose(emitContextDestroyed) {
            this.#pendingCommandNavigation?.reject(new UnknownErrorException('navigation canceled by context disposal'));
            this.#deleteAllChildren();
            this.#realmStorage.deleteRealms({
                browsingContextId: this.id,
            });
            // Delete context from the parent.
            if (!this.isTopLevelContext()) {
                this.parent.#children.delete(this.id);
            }
            // Fail all ongoing navigations.
            this.#failLifecycleIfNotFinished();
            if (emitContextDestroyed) {
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: BrowsingContext$2.EventNames.ContextDestroyed,
                    params: this.serializeToBidiValue(),
                }, this.id);
            }
            this.#eventManager.clearBufferedEvents(this.id);
            this.#browsingContextStorage.deleteContextById(this.id);
        }
        /** Returns the ID of this context. */
        get id() {
            return this.#id;
        }
        /** Returns the parent context ID. */
        get parentId() {
            return this.#parentId;
        }
        /** Sets the parent context ID and updates parent's children. */
        set parentId(parentId) {
            if (this.#parentId !== null) {
                this.#logger?.(LogType.debugError, 'Parent context already set');
                // Cannot do anything except logging, as throwing will stop event processing. So
                // just return,
                return;
            }
            this.#parentId = parentId;
            if (!this.isTopLevelContext()) {
                this.parent.addChild(this.id);
            }
        }
        /** Returns the parent context. */
        get parent() {
            if (this.parentId === null) {
                return null;
            }
            return this.#browsingContextStorage.getContext(this.parentId);
        }
        /** Returns all direct children contexts. */
        get directChildren() {
            return [...this.#children].map((id) => this.#browsingContextStorage.getContext(id));
        }
        /** Returns all children contexts, flattened. */
        get allChildren() {
            const children = this.directChildren;
            return children.concat(...children.map((child) => child.allChildren));
        }
        /**
         * Returns true if this is a top-level context.
         * This is the case whenever the parent context ID is null.
         */
        isTopLevelContext() {
            return this.#parentId === null;
        }
        get top() {
            // eslint-disable-next-line @typescript-eslint/no-this-alias
            let topContext = this;
            let parent = topContext.parent;
            while (parent) {
                topContext = parent;
                parent = topContext.parent;
            }
            return topContext;
        }
        addChild(childId) {
            this.#children.add(childId);
        }
        #deleteAllChildren(emitContextDestroyed = false) {
            this.directChildren.map((child) => child.dispose(emitContextDestroyed));
        }
        get cdpTarget() {
            return this.#cdpTarget;
        }
        updateCdpTarget(cdpTarget) {
            this.#cdpTarget = cdpTarget;
            this.#initListeners();
        }
        get url() {
            return this.#url;
        }
        async lifecycleLoaded() {
            await this.#lifecycle.load;
        }
        async targetUnblockedOrThrow() {
            const result = await this.#cdpTarget.unblocked;
            if (result.kind === 'error') {
                throw result.error;
            }
        }
        async getOrCreateSandbox(sandbox) {
            if (sandbox === undefined || sandbox === '') {
                // Default realm is not guaranteed to be created at this point, so return a deferred.
                return await this.#defaultRealmDeferred;
            }
            let maybeSandboxes = this.#realmStorage.findRealms({
                browsingContextId: this.id,
                sandbox,
            });
            if (maybeSandboxes.length === 0) {
                await this.#cdpTarget.cdpClient.sendCommand('Page.createIsolatedWorld', {
                    frameId: this.id,
                    worldName: sandbox,
                });
                // `Runtime.executionContextCreated` should be emitted by the time the
                // previous command is done.
                maybeSandboxes = this.#realmStorage.findRealms({
                    browsingContextId: this.id,
                    sandbox,
                });
                assert(maybeSandboxes.length !== 0);
            }
            // It's possible for more than one sandbox to be created due to provisional
            // frames. In this case, it's always the first one (i.e. the oldest one)
            // that is more relevant since the user may have set that one up already
            // through evaluation.
            return maybeSandboxes[0];
        }
        serializeToBidiValue(maxDepth = 0, addParentField = true) {
            return {
                context: this.#id,
                url: this.url,
                userContext: this.userContext,
                originalOpener: this.#originalOpener ?? null,
                // TODO(#2646): Implement Client Window correctly
                clientWindow: '',
                children: maxDepth > 0
                    ? this.directChildren.map((c) => c.serializeToBidiValue(maxDepth - 1, false))
                    : null,
                ...(addParentField ? { parent: this.#parentId } : {}),
            };
        }
        onTargetInfoChanged(params) {
            this.#url = params.targetInfo.url;
        }
        #initListeners() {
            this.#cdpTarget.cdpClient.on('Page.frameNavigated', (params) => {
                if (this.id !== params.frame.id) {
                    return;
                }
                this.#url = params.frame.url + (params.frame.urlFragment ?? '');
                this.#pendingNavigationUrl = undefined;
                // At the point the page is initialized, all the nested iframes from the
                // previous page are detached and realms are destroyed.
                // Delete children from context.
                this.#deleteAllChildren();
            });
            this.#cdpTarget.cdpClient.on('Page.navigatedWithinDocument', (params) => {
                if (this.id !== params.frameId) {
                    return;
                }
                this.#pendingNavigationUrl = undefined;
                const timestamp = _a$5.getTimestamp();
                this.#url = params.url;
                this.#navigation.withinDocument.resolve();
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: BrowsingContext$2.EventNames.FragmentNavigated,
                    params: {
                        context: this.id,
                        navigation: this.#navigationId,
                        timestamp,
                        url: this.#url,
                    },
                }, this.id);
            });
            this.#cdpTarget.cdpClient.on('Page.frameStartedLoading', (params) => {
                if (this.id !== params.frameId) {
                    return;
                }
                // Use `pendingNavigationId` if navigation initiated by BiDi
                // `BrowsingContext.navigate` or generate a new navigation id.
                this.#navigationId = this.#pendingNavigationId ?? uuidv4();
                this.#pendingNavigationId = undefined;
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: BrowsingContext$2.EventNames.NavigationStarted,
                    params: {
                        context: this.id,
                        navigation: this.#navigationId,
                        timestamp: _a$5.getTimestamp(),
                        // The URL of the navigation that is currently in progress. Although the URL
                        // is not yet known in case of user-initiated navigations, it is possible to
                        // provide the URL in case of BiDi-initiated navigations.
                        // TODO: provide proper URL in case of user-initiated navigations.
                        url: this.#pendingNavigationUrl ?? 'UNKNOWN',
                    },
                }, this.id);
            });
            // TODO: don't use deprecated `Page.frameScheduledNavigation` event.
            this.#cdpTarget.cdpClient.on('Page.frameScheduledNavigation', (params) => {
                if (this.id !== params.frameId) {
                    return;
                }
                this.#pendingNavigationUrl = params.url;
            });
            this.#cdpTarget.cdpClient.on('Page.frameRequestedNavigation', (params) => {
                if (this.id !== params.frameId) {
                    return;
                }
                // If there is a pending navigation, reject it.
                this.#pendingCommandNavigation?.reject(new UnknownErrorException(`navigation canceled, as new navigation is requested by ${params.reason}`));
                this.#pendingNavigationUrl = params.url;
            });
            this.#cdpTarget.cdpClient.on('Page.lifecycleEvent', (params) => {
                if (this.id !== params.frameId) {
                    return;
                }
                if (params.name === 'init') {
                    this.#documentChanged(params.loaderId);
                    return;
                }
                if (params.name === 'commit') {
                    this.#loaderId = params.loaderId;
                    return;
                }
                // If mapper attached to the page late, it might miss init and
                // commit events. In that case, save the first loaderId for this
                // frameId.
                if (!this.#loaderId) {
                    this.#loaderId = params.loaderId;
                }
                // Ignore event from not current navigation.
                if (params.loaderId !== this.#loaderId) {
                    return;
                }
                const timestamp = _a$5.getTimestamp();
                switch (params.name) {
                    case 'DOMContentLoaded':
                        this.#eventManager.registerEvent({
                            type: 'event',
                            method: BrowsingContext$2.EventNames.DomContentLoaded,
                            params: {
                                context: this.id,
                                navigation: this.#navigationId,
                                timestamp,
                                url: this.#url,
                            },
                        }, this.id);
                        this.#lifecycle.DOMContentLoaded.resolve();
                        break;
                    case 'load':
                        this.#eventManager.registerEvent({
                            type: 'event',
                            method: BrowsingContext$2.EventNames.Load,
                            params: {
                                context: this.id,
                                navigation: this.#navigationId,
                                timestamp,
                                url: this.#url,
                            },
                        }, this.id);
                        this.#lifecycle.load.resolve();
                        break;
                }
            });
            this.#cdpTarget.cdpClient.on('Runtime.executionContextCreated', (params) => {
                const { auxData, name, uniqueId, id } = params.context;
                if (!auxData || auxData.frameId !== this.id) {
                    return;
                }
                let origin;
                let sandbox;
                // Only these execution contexts are supported for now.
                switch (auxData.type) {
                    case 'isolated':
                        sandbox = name;
                        // Sandbox should have the same origin as the context itself, but in CDP
                        // it has an empty one.
                        if (!this.#defaultRealmDeferred.isFinished) {
                            this.#logger?.(LogType.debugError, 'Unexpectedly, isolated realm created before the default one');
                        }
                        origin = this.#defaultRealmDeferred.isFinished
                            ? this.#defaultRealmDeferred.result.origin
                            : // This fallback is not expected to be ever reached.
                                '';
                        break;
                    case 'default':
                        origin = serializeOrigin(params.context.origin);
                        break;
                    default:
                        return;
                }
                const realm = new WindowRealm(this.id, this.#browsingContextStorage, this.#cdpTarget.cdpClient, this.#eventManager, id, this.#logger, origin, uniqueId, this.#realmStorage, sandbox);
                if (auxData.isDefault) {
                    this.#defaultRealmDeferred.resolve(realm);
                    // Initialize ChannelProxy listeners for all the channels of all the
                    // preload scripts related to this BrowsingContext.
                    // TODO: extend for not default realms by the sandbox name.
                    void Promise.all(this.#cdpTarget
                        .getChannels()
                        .map((channel) => channel.startListenerFromWindow(realm, this.#eventManager)));
                }
            });
            this.#cdpTarget.cdpClient.on('Runtime.executionContextDestroyed', (params) => {
                if (this.#defaultRealmDeferred.isFinished &&
                    this.#defaultRealmDeferred.result.executionContextId ===
                        params.executionContextId) {
                    this.#defaultRealmDeferred = new Deferred();
                }
                this.#realmStorage.deleteRealms({
                    cdpSessionId: this.#cdpTarget.cdpSessionId,
                    executionContextId: params.executionContextId,
                });
            });
            this.#cdpTarget.cdpClient.on('Runtime.executionContextsCleared', () => {
                if (!this.#defaultRealmDeferred.isFinished) {
                    this.#defaultRealmDeferred.reject(new UnknownErrorException('execution contexts cleared'));
                }
                this.#defaultRealmDeferred = new Deferred();
                this.#realmStorage.deleteRealms({
                    cdpSessionId: this.#cdpTarget.cdpSessionId,
                });
            });
            this.#cdpTarget.cdpClient.on('Page.javascriptDialogClosed', (params) => {
                const accepted = params.result;
                if (this.#lastUserPromptType === undefined) {
                    this.#logger?.(LogType.debugError, 'Unexpectedly no opening prompt event before closing one');
                }
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: BrowsingContext$2.EventNames.UserPromptClosed,
                    params: {
                        context: this.id,
                        accepted,
                        // `lastUserPromptType` should never be undefined here, so fallback to
                        // `UNKNOWN`. The fallback is required to prevent tests from hanging while
                        // waiting for the closing event. The cast is required, as the `UNKNOWN` value
                        // is not standard.
                        type: this.#lastUserPromptType ??
                            'UNKNOWN',
                        userText: accepted && params.userInput ? params.userInput : undefined,
                    },
                }, this.id);
                this.#lastUserPromptType = undefined;
            });
            this.#cdpTarget.cdpClient.on('Page.javascriptDialogOpening', (params) => {
                const promptType = _a$5.#getPromptType(params.type);
                // Set the last prompt type to provide it in closing event.
                this.#lastUserPromptType = promptType;
                const promptHandler = this.#getPromptHandler(promptType);
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: BrowsingContext$2.EventNames.UserPromptOpened,
                    params: {
                        context: this.id,
                        handler: promptHandler,
                        type: promptType,
                        message: params.message,
                        ...(params.type === 'prompt'
                            ? { defaultValue: params.defaultPrompt }
                            : {}),
                    },
                }, this.id);
                switch (promptHandler) {
                    // Based on `unhandledPromptBehavior`, check if the prompt should be handled
                    // automatically (`accept`, `dismiss`) or wait for the user to do it.
                    case "accept" /* Session.UserPromptHandlerType.Accept */:
                        void this.handleUserPrompt(true);
                        break;
                    case "dismiss" /* Session.UserPromptHandlerType.Dismiss */:
                        void this.handleUserPrompt(false);
                        break;
                }
            });
        }
        static #getPromptType(cdpType) {
            switch (cdpType) {
                case 'alert':
                    return "alert" /* BrowsingContext.UserPromptType.Alert */;
                case 'beforeunload':
                    return "beforeunload" /* BrowsingContext.UserPromptType.Beforeunload */;
                case 'confirm':
                    return "confirm" /* BrowsingContext.UserPromptType.Confirm */;
                case 'prompt':
                    return "prompt" /* BrowsingContext.UserPromptType.Prompt */;
            }
        }
        #getPromptHandler(promptType) {
            const defaultPromptHandler = "dismiss" /* Session.UserPromptHandlerType.Dismiss */;
            switch (promptType) {
                case "alert" /* BrowsingContext.UserPromptType.Alert */:
                    return (this.#unhandledPromptBehavior?.alert ??
                        this.#unhandledPromptBehavior?.default ??
                        defaultPromptHandler);
                case "beforeunload" /* BrowsingContext.UserPromptType.Beforeunload */:
                    return (this.#unhandledPromptBehavior?.beforeUnload ??
                        this.#unhandledPromptBehavior?.default ??
                        "accept" /* Session.UserPromptHandlerType.Accept */);
                case "confirm" /* BrowsingContext.UserPromptType.Confirm */:
                    return (this.#unhandledPromptBehavior?.confirm ??
                        this.#unhandledPromptBehavior?.default ??
                        defaultPromptHandler);
                case "prompt" /* BrowsingContext.UserPromptType.Prompt */:
                    return (this.#unhandledPromptBehavior?.prompt ??
                        this.#unhandledPromptBehavior?.default ??
                        defaultPromptHandler);
            }
        }
        #documentChanged(loaderId) {
            if (loaderId === undefined || this.#loaderId === loaderId) {
                // Same document navigation. Document didn't change.
                if (this.#navigation.withinDocument.isFinished) {
                    this.#navigation.withinDocument = new Deferred();
                }
                else {
                    this.#logger?.(_a$5.LOGGER_PREFIX, 'Document changed (navigatedWithinDocument)');
                }
                return;
            }
            // Document changed.
            this.#resetLifecycleIfFinished();
            this.#loaderId = loaderId;
            // Delete all child iframes and notify about top level destruction.
            this.#deleteAllChildren(true);
        }
        #resetLifecycleIfFinished() {
            if (this.#lifecycle.DOMContentLoaded.isFinished) {
                this.#lifecycle.DOMContentLoaded = new Deferred();
            }
            else {
                this.#logger?.(_a$5.LOGGER_PREFIX, 'Document changed (DOMContentLoaded)');
            }
            if (this.#lifecycle.load.isFinished) {
                this.#lifecycle.load = new Deferred();
            }
            else {
                this.#logger?.(_a$5.LOGGER_PREFIX, 'Document changed (load)');
            }
        }
        #failLifecycleIfNotFinished() {
            if (!this.#lifecycle.DOMContentLoaded.isFinished) {
                this.#lifecycle.DOMContentLoaded.reject(new UnknownErrorException('navigation canceled'));
            }
            if (!this.#lifecycle.load.isFinished) {
                this.#lifecycle.load.reject(new UnknownErrorException('navigation canceled'));
            }
        }
        async navigate(url, wait) {
            try {
                new URL(url);
            }
            catch {
                throw new InvalidArgumentException(`Invalid URL: ${url}`);
            }
            this.#pendingCommandNavigation?.reject(new UnknownErrorException('navigation canceled by concurrent navigation'));
            await this.targetUnblockedOrThrow();
            // Set the pending navigation URL to provide it in `browsingContext.navigationStarted`
            // event.
            // TODO: detect navigation start not from CDP. Check if
            //  `Page.frameRequestedNavigation` can be used for this purpose.
            this.#pendingNavigationUrl = url;
            const navigationId = uuidv4();
            this.#pendingNavigationId = navigationId;
            this.#pendingCommandNavigation = new Deferred();
            // Navigate and wait for the result. If the navigation fails, the error event is
            // emitted and the promise is rejected.
            const cdpNavigatePromise = (async () => {
                const cdpNavigateResult = await this.#cdpTarget.cdpClient.sendCommand('Page.navigate', {
                    url,
                    frameId: this.id,
                });
                if (cdpNavigateResult.errorText) {
                    // If navigation failed, no pending navigation is left.
                    this.#pendingNavigationUrl = undefined;
                    this.#eventManager.registerEvent({
                        type: 'event',
                        method: BrowsingContext$2.EventNames.NavigationFailed,
                        params: {
                            context: this.id,
                            navigation: navigationId,
                            timestamp: _a$5.getTimestamp(),
                            url,
                        },
                    }, this.id);
                    throw new UnknownErrorException(cdpNavigateResult.errorText);
                }
                this.#documentChanged(cdpNavigateResult.loaderId);
                return cdpNavigateResult;
            })();
            if (wait === "none" /* BrowsingContext.ReadinessState.None */) {
                // Do not wait for the result of the navigation promise.
                this.#pendingCommandNavigation.resolve();
                this.#pendingCommandNavigation = undefined;
                return {
                    navigation: navigationId,
                    url,
                };
            }
            const cdpNavigateResult = await cdpNavigatePromise;
            // Wait for either the navigation is finished or canceled by another navigation.
            await Promise.race([
                // No `loaderId` means same-document navigation.
                this.#waitNavigation(wait, cdpNavigateResult.loaderId === undefined),
                // Throw an error if the navigation is canceled.
                this.#pendingCommandNavigation,
            ]);
            this.#pendingCommandNavigation.resolve();
            this.#pendingCommandNavigation = undefined;
            return {
                navigation: navigationId,
                // Url can change due to redirect get the latest one.
                url: this.#url,
            };
        }
        async #waitNavigation(wait, withinDocument) {
            if (withinDocument) {
                await this.#navigation.withinDocument;
                return;
            }
            switch (wait) {
                case "none" /* BrowsingContext.ReadinessState.None */:
                    return;
                case "interactive" /* BrowsingContext.ReadinessState.Interactive */:
                    await this.#lifecycle.DOMContentLoaded;
                    return;
                case "complete" /* BrowsingContext.ReadinessState.Complete */:
                    await this.#lifecycle.load;
                    return;
            }
        }
        // TODO: support concurrent navigations analogous to `navigate`.
        async reload(ignoreCache, wait) {
            await this.targetUnblockedOrThrow();
            this.#resetLifecycleIfFinished();
            await this.#cdpTarget.cdpClient.sendCommand('Page.reload', {
                ignoreCache,
            });
            switch (wait) {
                case "none" /* BrowsingContext.ReadinessState.None */:
                    break;
                case "interactive" /* BrowsingContext.ReadinessState.Interactive */:
                    await this.#lifecycle.DOMContentLoaded;
                    break;
                case "complete" /* BrowsingContext.ReadinessState.Complete */:
                    await this.#lifecycle.load;
                    break;
            }
            return {
                navigation: this.#navigationId,
                url: this.url,
            };
        }
        async setViewport(viewport, devicePixelRatio) {
            if (viewport === null && devicePixelRatio === null) {
                await this.#cdpTarget.cdpClient.sendCommand('Emulation.clearDeviceMetricsOverride');
            }
            else {
                try {
                    let appliedViewport;
                    if (viewport === undefined) {
                        appliedViewport = this.#previousViewport;
                    }
                    else if (viewport === null) {
                        appliedViewport = {
                            width: 0,
                            height: 0,
                        };
                    }
                    else {
                        appliedViewport = viewport;
                    }
                    this.#previousViewport = appliedViewport;
                    await this.#cdpTarget.cdpClient.sendCommand('Emulation.setDeviceMetricsOverride', {
                        width: this.#previousViewport.width,
                        height: this.#previousViewport.height,
                        deviceScaleFactor: devicePixelRatio ? devicePixelRatio : 0,
                        mobile: false,
                        dontSetVisibleSize: true,
                    });
                }
                catch (err) {
                    if (err.message.startsWith(
                    // https://crsrc.org/c/content/browser/devtools/protocol/emulation_handler.cc;l=257;drc=2f6eee84cf98d4227e7c41718dd71b82f26d90ff
                    'Width and height values must be positive')) {
                        throw new UnsupportedOperationException('Provided viewport dimensions are not supported');
                    }
                    throw err;
                }
            }
        }
        async handleUserPrompt(accept, userText) {
            await this.#cdpTarget.cdpClient.sendCommand('Page.handleJavaScriptDialog', {
                accept: accept ?? true,
                promptText: userText,
            });
        }
        async activate() {
            await this.#cdpTarget.cdpClient.sendCommand('Page.bringToFront');
        }
        async captureScreenshot(params) {
            if (!this.isTopLevelContext()) {
                throw new UnsupportedOperationException(`Non-top-level 'context' (${params.context}) is currently not supported`);
            }
            const formatParameters = getImageFormatParameters(params);
            // XXX: Focus the original tab after the screenshot is taken.
            // This is needed because the screenshot gets blocked until the active tab gets focus.
            await this.#cdpTarget.cdpClient.sendCommand('Page.bringToFront');
            let captureBeyondViewport = false;
            let script;
            params.origin ??= 'viewport';
            switch (params.origin) {
                case 'document': {
                    script = String(() => {
                        const element = document.documentElement;
                        return {
                            x: 0,
                            y: 0,
                            width: element.scrollWidth,
                            height: element.scrollHeight,
                        };
                    });
                    captureBeyondViewport = true;
                    break;
                }
                case 'viewport': {
                    script = String(() => {
                        const viewport = window.visualViewport;
                        return {
                            x: viewport.pageLeft,
                            y: viewport.pageTop,
                            width: viewport.width,
                            height: viewport.height,
                        };
                    });
                    break;
                }
            }
            const realm = await this.getOrCreateSandbox(undefined);
            const originResult = await realm.callFunction(script, false);
            assert(originResult.type === 'success');
            const origin = deserializeDOMRect(originResult.result);
            assert(origin);
            let rect = origin;
            if (params.clip) {
                const clip = params.clip;
                if (params.origin === 'viewport' && clip.type === 'box') {
                    // For viewport origin, the clip is relative to the viewport, while the CDP
                    // screenshot is relative to the document. So correction for the viewport position
                    // is required.
                    clip.x += origin.x;
                    clip.y += origin.y;
                }
                rect = getIntersectionRect(await this.#parseRect(clip), origin);
            }
            if (rect.width === 0 || rect.height === 0) {
                throw new UnableToCaptureScreenException(`Unable to capture screenshot with zero dimensions: width=${rect.width}, height=${rect.height}`);
            }
            return await this.#cdpTarget.cdpClient.sendCommand('Page.captureScreenshot', {
                clip: { ...rect, scale: 1.0 },
                ...formatParameters,
                captureBeyondViewport,
            });
        }
        async print(params) {
            const cdpParams = {};
            if (params.background !== undefined) {
                cdpParams.printBackground = params.background;
            }
            if (params.margin?.bottom !== undefined) {
                cdpParams.marginBottom = inchesFromCm(params.margin.bottom);
            }
            if (params.margin?.left !== undefined) {
                cdpParams.marginLeft = inchesFromCm(params.margin.left);
            }
            if (params.margin?.right !== undefined) {
                cdpParams.marginRight = inchesFromCm(params.margin.right);
            }
            if (params.margin?.top !== undefined) {
                cdpParams.marginTop = inchesFromCm(params.margin.top);
            }
            if (params.orientation !== undefined) {
                cdpParams.landscape = params.orientation === 'landscape';
            }
            if (params.page?.height !== undefined) {
                cdpParams.paperHeight = inchesFromCm(params.page.height);
            }
            if (params.page?.width !== undefined) {
                cdpParams.paperWidth = inchesFromCm(params.page.width);
            }
            if (params.pageRanges !== undefined) {
                for (const range of params.pageRanges) {
                    if (typeof range === 'number') {
                        continue;
                    }
                    const rangeParts = range.split('-');
                    if (rangeParts.length < 1 || rangeParts.length > 2) {
                        throw new InvalidArgumentException(`Invalid page range: ${range} is not a valid integer range.`);
                    }
                    if (rangeParts.length === 1) {
                        void parseInteger(rangeParts[0] ?? '');
                        continue;
                    }
                    let lowerBound;
                    let upperBound;
                    const [rangeLowerPart = '', rangeUpperPart = ''] = rangeParts;
                    if (rangeLowerPart === '') {
                        lowerBound = 1;
                    }
                    else {
                        lowerBound = parseInteger(rangeLowerPart);
                    }
                    if (rangeUpperPart === '') {
                        upperBound = Number.MAX_SAFE_INTEGER;
                    }
                    else {
                        upperBound = parseInteger(rangeUpperPart);
                    }
                    if (lowerBound > upperBound) {
                        throw new InvalidArgumentException(`Invalid page range: ${rangeLowerPart} > ${rangeUpperPart}`);
                    }
                }
                cdpParams.pageRanges = params.pageRanges.join(',');
            }
            if (params.scale !== undefined) {
                cdpParams.scale = params.scale;
            }
            if (params.shrinkToFit !== undefined) {
                cdpParams.preferCSSPageSize = !params.shrinkToFit;
            }
            try {
                const result = await this.#cdpTarget.cdpClient.sendCommand('Page.printToPDF', cdpParams);
                return {
                    data: result.data,
                };
            }
            catch (error) {
                // Effectively zero dimensions.
                if (error.message ===
                    'invalid print parameters: content area is empty') {
                    throw new UnsupportedOperationException(error.message);
                }
                throw error;
            }
        }
        /**
         * See
         * https://w3c.github.io/webdriver-bidi/#:~:text=If%20command%20parameters%20contains%20%22clip%22%3A
         */
        async #parseRect(clip) {
            switch (clip.type) {
                case 'box':
                    return { x: clip.x, y: clip.y, width: clip.width, height: clip.height };
                case 'element': {
                    // TODO: #1213: Use custom sandbox specifically for Chromium BiDi
                    const sandbox = await this.getOrCreateSandbox(undefined);
                    const result = await sandbox.callFunction(String((element) => {
                        return element instanceof Element;
                    }), false, { type: 'undefined' }, [clip.element]);
                    if (result.type === 'exception') {
                        throw new NoSuchElementException(`Element '${clip.element.sharedId}' was not found`);
                    }
                    assert(result.result.type === 'boolean');
                    if (!result.result.value) {
                        throw new NoSuchElementException(`Node '${clip.element.sharedId}' is not an Element`);
                    }
                    {
                        const result = await sandbox.callFunction(String((element) => {
                            const rect = element.getBoundingClientRect();
                            return {
                                x: rect.x,
                                y: rect.y,
                                height: rect.height,
                                width: rect.width,
                            };
                        }), false, { type: 'undefined' }, [clip.element]);
                        assert(result.type === 'success');
                        const rect = deserializeDOMRect(result.result);
                        if (!rect) {
                            throw new UnableToCaptureScreenException(`Could not get bounding box for Element '${clip.element.sharedId}'`);
                        }
                        return rect;
                    }
                }
            }
        }
        async close() {
            await this.#cdpTarget.cdpClient.sendCommand('Page.close');
        }
        async traverseHistory(delta) {
            if (delta === 0) {
                return;
            }
            const history = await this.#cdpTarget.cdpClient.sendCommand('Page.getNavigationHistory');
            const entry = history.entries[history.currentIndex + delta];
            if (!entry) {
                throw new NoSuchHistoryEntryException(`No history entry at delta ${delta}`);
            }
            await this.#cdpTarget.cdpClient.sendCommand('Page.navigateToHistoryEntry', {
                entryId: entry.id,
            });
        }
        async toggleModulesIfNeeded() {
            await Promise.all([
                this.#cdpTarget.toggleNetworkIfNeeded(),
                this.#cdpTarget.toggleDeviceAccessIfNeeded(),
            ]);
        }
        async locateNodes(params) {
            // TODO: create a dedicated sandbox instead of `#defaultRealm`.
            return await this.#locateNodesByLocator(await this.#defaultRealmDeferred, params.locator, params.startNodes ?? [], params.maxNodeCount, params.serializationOptions);
        }
        async #getLocatorDelegate(realm, locator, maxNodeCount, startNodes) {
            switch (locator.type) {
                case 'css':
                    return {
                        functionDeclaration: String((cssSelector, maxNodeCount, ...startNodes) => {
                            const locateNodesUsingCss = (element) => {
                                if (!(element instanceof HTMLElement ||
                                    element instanceof Document ||
                                    element instanceof DocumentFragment)) {
                                    throw new Error('startNodes in css selector should be HTMLElement, Document or DocumentFragment');
                                }
                                return [...element.querySelectorAll(cssSelector)];
                            };
                            startNodes = startNodes.length > 0 ? startNodes : [document];
                            const returnedNodes = startNodes
                                .map((startNode) => 
                            // TODO: stop search early if `maxNodeCount` is reached.
                            locateNodesUsingCss(startNode))
                                .flat(1);
                            return maxNodeCount === 0
                                ? returnedNodes
                                : returnedNodes.slice(0, maxNodeCount);
                        }),
                        argumentsLocalValues: [
                            // `cssSelector`
                            { type: 'string', value: locator.value },
                            // `maxNodeCount` with `0` means no limit.
                            { type: 'number', value: maxNodeCount ?? 0 },
                            // `startNodes`
                            ...startNodes,
                        ],
                    };
                case 'xpath':
                    return {
                        functionDeclaration: String((xPathSelector, maxNodeCount, ...startNodes) => {
                            // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-xpath
                            const evaluator = new XPathEvaluator();
                            const expression = evaluator.createExpression(xPathSelector);
                            const locateNodesUsingXpath = (element) => {
                                const xPathResult = expression.evaluate(element, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
                                const returnedNodes = [];
                                for (let i = 0; i < xPathResult.snapshotLength; i++) {
                                    returnedNodes.push(xPathResult.snapshotItem(i));
                                }
                                return returnedNodes;
                            };
                            startNodes = startNodes.length > 0 ? startNodes : [document];
                            const returnedNodes = startNodes
                                .map((startNode) => 
                            // TODO: stop search early if `maxNodeCount` is reached.
                            locateNodesUsingXpath(startNode))
                                .flat(1);
                            return maxNodeCount === 0
                                ? returnedNodes
                                : returnedNodes.slice(0, maxNodeCount);
                        }),
                        argumentsLocalValues: [
                            // `xPathSelector`
                            { type: 'string', value: locator.value },
                            // `maxNodeCount` with `0` means no limit.
                            { type: 'number', value: maxNodeCount ?? 0 },
                            // `startNodes`
                            ...startNodes,
                        ],
                    };
                case 'innerText':
                    // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-inner-text
                    if (locator.value === '') {
                        throw new InvalidSelectorException('innerText locator cannot be empty');
                    }
                    return {
                        functionDeclaration: String((innerTextSelector, fullMatch, ignoreCase, maxNodeCount, maxDepth, ...startNodes) => {
                            const searchText = ignoreCase
                                ? innerTextSelector.toUpperCase()
                                : innerTextSelector;
                            const locateNodesUsingInnerText = (node, currentMaxDepth) => {
                                const returnedNodes = [];
                                if (node instanceof DocumentFragment ||
                                    node instanceof Document) {
                                    const children = [...node.children];
                                    children.forEach((child) => 
                                    // `currentMaxDepth` is not decremented intentionally according to
                                    // https://github.com/w3c/webdriver-bidi/pull/713.
                                    returnedNodes.push(...locateNodesUsingInnerText(child, currentMaxDepth)));
                                    return returnedNodes;
                                }
                                if (!(node instanceof HTMLElement)) {
                                    return [];
                                }
                                const element = node;
                                const nodeInnerText = ignoreCase
                                    ? element.innerText?.toUpperCase()
                                    : element.innerText;
                                if (!nodeInnerText.includes(searchText)) {
                                    return [];
                                }
                                const childNodes = [];
                                for (const child of element.children) {
                                    if (child instanceof HTMLElement) {
                                        childNodes.push(child);
                                    }
                                }
                                if (childNodes.length === 0) {
                                    if (fullMatch && nodeInnerText === searchText) {
                                        returnedNodes.push(element);
                                    }
                                    else {
                                        if (!fullMatch) {
                                            // Note: `nodeInnerText.includes(searchText)` is already checked
                                            returnedNodes.push(element);
                                        }
                                    }
                                }
                                else {
                                    const childNodeMatches = 
                                    // Don't search deeper if `maxDepth` is reached.
                                    currentMaxDepth <= 0
                                        ? []
                                        : childNodes
                                            .map((child) => locateNodesUsingInnerText(child, currentMaxDepth - 1))
                                            .flat(1);
                                    if (childNodeMatches.length === 0) {
                                        // Note: `nodeInnerText.includes(searchText)` is already checked
                                        if (!fullMatch || nodeInnerText === searchText) {
                                            returnedNodes.push(element);
                                        }
                                    }
                                    else {
                                        returnedNodes.push(...childNodeMatches);
                                    }
                                }
                                // TODO: stop search early if `maxNodeCount` is reached.
                                return returnedNodes;
                            };
                            // TODO: stop search early if `maxNodeCount` is reached.
                            startNodes = startNodes.length > 0 ? startNodes : [document];
                            const returnedNodes = startNodes
                                .map((startNode) => 
                            // TODO: stop search early if `maxNodeCount` is reached.
                            locateNodesUsingInnerText(startNode, maxDepth))
                                .flat(1);
                            return maxNodeCount === 0
                                ? returnedNodes
                                : returnedNodes.slice(0, maxNodeCount);
                        }),
                        argumentsLocalValues: [
                            // `innerTextSelector`
                            { type: 'string', value: locator.value },
                            // `fullMatch` with default `true`.
                            { type: 'boolean', value: locator.matchType !== 'partial' },
                            // `ignoreCase` with default `false`.
                            { type: 'boolean', value: locator.ignoreCase === true },
                            // `maxNodeCount` with `0` means no limit.
                            { type: 'number', value: maxNodeCount ?? 0 },
                            // `maxDepth` with default `1000` (same as default full serialization depth).
                            { type: 'number', value: locator.maxDepth ?? 1000 },
                            // `startNodes`
                            ...startNodes,
                        ],
                    };
                case 'accessibility': {
                    // https://w3c.github.io/webdriver-bidi/#locate-nodes-using-accessibility-attributes
                    if (!locator.value.name && !locator.value.role) {
                        throw new InvalidSelectorException('Either name or role has to be specified');
                    }
                    // The next two commands cause a11y caches for the target to be
                    // preserved. We probably do not need to disable them if the
                    // client is using a11y features, but we could by calling
                    // Accessibility.disable.
                    await Promise.all([
                        this.#cdpTarget.cdpClient.sendCommand('Accessibility.enable'),
                        this.#cdpTarget.cdpClient.sendCommand('Accessibility.getRootAXNode'),
                    ]);
                    const bindings = await realm.evaluate(
                    /* expression=*/ '({getAccessibleName, getAccessibleRole})', 
                    /* awaitPromise=*/ false, "root" /* Script.ResultOwnership.Root */, 
                    /* serializationOptions= */ undefined, 
                    /* userActivation=*/ false, 
                    /* includeCommandLineApi=*/ true);
                    if (bindings.type !== 'success') {
                        throw new Error('Could not get bindings');
                    }
                    if (bindings.result.type !== 'object') {
                        throw new Error('Could not get bindings');
                    }
                    return {
                        functionDeclaration: String((name, role, bindings, maxNodeCount, ...startNodes) => {
                            const returnedNodes = [];
                            let aborted = false;
                            function collect(contextNodes, selector) {
                                if (aborted) {
                                    return;
                                }
                                for (const contextNode of contextNodes) {
                                    let match = true;
                                    if (selector.role) {
                                        const role = bindings.getAccessibleRole(contextNode);
                                        if (selector.role !== role) {
                                            match = false;
                                        }
                                    }
                                    if (selector.name) {
                                        const name = bindings.getAccessibleName(contextNode);
                                        if (selector.name !== name) {
                                            match = false;
                                        }
                                    }
                                    if (match) {
                                        if (maxNodeCount !== 0 &&
                                            returnedNodes.length === maxNodeCount) {
                                            aborted = true;
                                            break;
                                        }
                                        returnedNodes.push(contextNode);
                                    }
                                    const childNodes = [];
                                    for (const child of contextNode.children) {
                                        if (child instanceof HTMLElement) {
                                            childNodes.push(child);
                                        }
                                    }
                                    collect(childNodes, selector);
                                }
                            }
                            startNodes =
                                startNodes.length > 0
                                    ? startNodes
                                    : Array.from(document.documentElement.children).filter((c) => c instanceof HTMLElement);
                            collect(startNodes, {
                                role,
                                name,
                            });
                            return returnedNodes;
                        }),
                        argumentsLocalValues: [
                            // `name`
                            { type: 'string', value: locator.value.name || '' },
                            // `role`
                            { type: 'string', value: locator.value.role || '' },
                            // `bindings`.
                            { handle: bindings.result.handle },
                            // `maxNodeCount` with `0` means no limit.
                            { type: 'number', value: maxNodeCount ?? 0 },
                            // `startNodes`
                            ...startNodes,
                        ],
                    };
                }
            }
        }
        async #locateNodesByLocator(realm, locator, startNodes, maxNodeCount, serializationOptions) {
            const locatorDelegate = await this.#getLocatorDelegate(realm, locator, maxNodeCount, startNodes);
            serializationOptions = {
                ...serializationOptions,
                // The returned object is an array of nodes, so no need in deeper JS serialization.
                maxObjectDepth: 1,
            };
            const locatorResult = await realm.callFunction(locatorDelegate.functionDeclaration, false, { type: 'undefined' }, locatorDelegate.argumentsLocalValues, "none" /* Script.ResultOwnership.None */, serializationOptions);
            if (locatorResult.type !== 'success') {
                this.#logger?.(_a$5.LOGGER_PREFIX, 'Failed locateNodesByLocator', locatorResult);
                // Heuristic to detect invalid selector for different types of selectors.
                if (
                // CSS selector.
                locatorResult.exceptionDetails.text?.endsWith('is not a valid selector.') ||
                    // XPath selector.
                    locatorResult.exceptionDetails.text?.endsWith('is not a valid XPath expression.')) {
                    throw new InvalidSelectorException(`Not valid selector ${typeof locator.value === 'string' ? locator.value : JSON.stringify(locator.value)}`);
                }
                // Heuristic to detect if the `startNode` is not an `HTMLElement` in css selector.
                if (locatorResult.exceptionDetails.text ===
                    'Error: startNodes in css selector should be HTMLElement, Document or DocumentFragment') {
                    throw new InvalidArgumentException('startNodes in css selector should be HTMLElement, Document or DocumentFragment');
                }
                throw new UnknownErrorException(`Unexpected error in selector script: ${locatorResult.exceptionDetails.text}`);
            }
            if (locatorResult.result.type !== 'array') {
                throw new UnknownErrorException(`Unexpected selector script result type: ${locatorResult.result.type}`);
            }
            // Check there are no non-node elements in the result.
            const nodes = locatorResult.result.value.map((value) => {
                if (value.type !== 'node') {
                    throw new UnknownErrorException(`Unexpected selector script result element: ${value.type}`);
                }
                return value;
            });
            return { nodes };
        }
    }
    _a$5 = BrowsingContextImpl;
    function serializeOrigin(origin) {
        // https://html.spec.whatwg.org/multipage/origin.html#ascii-serialisation-of-an-origin
        if (['://', ''].includes(origin)) {
            origin = 'null';
        }
        return origin;
    }
    function getImageFormatParameters(params) {
        const { quality, type } = params.format ?? {
            type: 'image/png',
        };
        switch (type) {
            case 'image/png': {
                return { format: 'png' };
            }
            case 'image/jpeg': {
                return {
                    format: 'jpeg',
                    ...(quality === undefined ? {} : { quality: Math.round(quality * 100) }),
                };
            }
            case 'image/webp': {
                return {
                    format: 'webp',
                    ...(quality === undefined ? {} : { quality: Math.round(quality * 100) }),
                };
            }
        }
        throw new InvalidArgumentException(`Image format '${type}' is not a supported format`);
    }
    function deserializeDOMRect(result) {
        if (result.type !== 'object' || result.value === undefined) {
            return;
        }
        const x = result.value.find(([key]) => {
            return key === 'x';
        })?.[1];
        const y = result.value.find(([key]) => {
            return key === 'y';
        })?.[1];
        const height = result.value.find(([key]) => {
            return key === 'height';
        })?.[1];
        const width = result.value.find(([key]) => {
            return key === 'width';
        })?.[1];
        if (x?.type !== 'number' ||
            y?.type !== 'number' ||
            height?.type !== 'number' ||
            width?.type !== 'number') {
            return;
        }
        return {
            x: x.value,
            y: y.value,
            width: width.value,
            height: height.value,
        };
    }
    /** @see https://w3c.github.io/webdriver-bidi/#normalize-rect */
    function normalizeRect(box) {
        return {
            ...(box.width < 0
                ? {
                    x: box.x + box.width,
                    width: -box.width,
                }
                : {
                    x: box.x,
                    width: box.width,
                }),
            ...(box.height < 0
                ? {
                    y: box.y + box.height,
                    height: -box.height,
                }
                : {
                    y: box.y,
                    height: box.height,
                }),
        };
    }
    /** @see https://w3c.github.io/webdriver-bidi/#rectangle-intersection */
    function getIntersectionRect(first, second) {
        first = normalizeRect(first);
        second = normalizeRect(second);
        const x = Math.max(first.x, second.x);
        const y = Math.max(first.y, second.y);
        return {
            x,
            y,
            width: Math.max(Math.min(first.x + first.width, second.x + second.width) - x, 0),
            height: Math.max(Math.min(first.y + first.height, second.y + second.height) - y, 0),
        };
    }
    function parseInteger(value) {
        value = value.trim();
        if (!/^[0-9]+$/.test(value)) {
            throw new InvalidArgumentException(`Invalid integer: ${value}`);
        }
        return parseInt(value);
    }

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class WorkerRealm extends Realm {
        #realmType;
        #ownerRealms;
        constructor(cdpClient, eventManager, executionContextId, logger, origin, ownerRealms, realmId, realmStorage, realmType) {
            super(cdpClient, eventManager, executionContextId, logger, origin, realmId, realmStorage);
            this.#ownerRealms = ownerRealms;
            this.#realmType = realmType;
            this.initialize();
        }
        get associatedBrowsingContexts() {
            return this.#ownerRealms.flatMap((realm) => realm.associatedBrowsingContexts);
        }
        get realmType() {
            return this.#realmType;
        }
        get source() {
            return {
                realm: this.realmId,
                // This is a hack to make Puppeteer able to track workers.
                // TODO: remove after Puppeteer tracks workers by owners and use the base version.
                context: this.associatedBrowsingContexts[0]?.id,
            };
        }
        get realmInfo() {
            const owners = this.#ownerRealms.map((realm) => realm.realmId);
            const { realmType } = this;
            switch (realmType) {
                case 'dedicated-worker': {
                    const owner = owners[0];
                    if (owner === undefined || owners.length !== 1) {
                        throw new Error('Dedicated worker must have exactly one owner');
                    }
                    return {
                        ...this.baseInfo,
                        type: realmType,
                        owners: [owner],
                    };
                }
                case 'service-worker':
                case 'shared-worker': {
                    return {
                        ...this.baseInfo,
                        type: realmType,
                    };
                }
            }
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    const specifiers = ['%s', '%d', '%i', '%f', '%o', '%O', '%c'];
    function isFormatSpecifier(str) {
        return specifiers.some((spec) => str.includes(spec));
    }
    /**
     * @param args input remote values to be format printed
     * @return parsed text of the remote values in specific format
     */
    function logMessageFormatter(args) {
        let output = '';
        const argFormat = args[0].value.toString();
        const argValues = args.slice(1, undefined);
        const tokens = argFormat.split(new RegExp(specifiers.map((spec) => `(${spec})`).join('|'), 'g'));
        for (const token of tokens) {
            if (token === undefined || token === '') {
                continue;
            }
            if (isFormatSpecifier(token)) {
                const arg = argValues.shift();
                // raise an exception when less value is provided
                assert(arg, `Less value is provided: "${getRemoteValuesText(args, false)}"`);
                if (token === '%s') {
                    output += stringFromArg(arg);
                }
                else if (token === '%d' || token === '%i') {
                    if (arg.type === 'bigint' ||
                        arg.type === 'number' ||
                        arg.type === 'string') {
                        output += parseInt(arg.value.toString(), 10);
                    }
                    else {
                        output += 'NaN';
                    }
                }
                else if (token === '%f') {
                    if (arg.type === 'bigint' ||
                        arg.type === 'number' ||
                        arg.type === 'string') {
                        output += parseFloat(arg.value.toString());
                    }
                    else {
                        output += 'NaN';
                    }
                }
                else {
                    // %o, %O, %c
                    output += toJson(arg);
                }
            }
            else {
                output += token;
            }
        }
        // raise an exception when more value is provided
        if (argValues.length > 0) {
            throw new Error(`More value is provided: "${getRemoteValuesText(args, false)}"`);
        }
        return output;
    }
    /**
     * @param arg input remote value to be parsed
     * @return parsed text of the remote value
     *
     * input: {"type": "number", "value": 1}
     * output: 1
     *
     * input: {"type": "string", "value": "abc"}
     * output: "abc"
     *
     * input: {"type": "object",  "value": [["id", {"type": "number", "value": 1}]]}
     * output: '{"id": 1}'
     *
     * input: {"type": "object", "value": [["font-size", {"type": "string", "value": "20px"}]]}
     * output: '{"font-size": "20px"}'
     */
    function toJson(arg) {
        // arg type validation
        if (arg.type !== 'array' &&
            arg.type !== 'bigint' &&
            arg.type !== 'date' &&
            arg.type !== 'number' &&
            arg.type !== 'object' &&
            arg.type !== 'string') {
            return stringFromArg(arg);
        }
        if (arg.type === 'bigint') {
            return `${arg.value.toString()}n`;
        }
        if (arg.type === 'number') {
            return arg.value.toString();
        }
        if (['date', 'string'].includes(arg.type)) {
            return JSON.stringify(arg.value);
        }
        if (arg.type === 'object') {
            return `{${arg.value
            .map((pair) => {
            return `${JSON.stringify(pair[0])}:${toJson(pair[1])}`;
        })
            .join(',')}}`;
        }
        if (arg.type === 'array') {
            return `[${arg.value?.map((val) => toJson(val)).join(',') ?? ''}]`;
        }
        // eslint-disable-next-line @typescript-eslint/no-base-to-string
        throw Error(`Invalid value type: ${arg}`);
    }
    function stringFromArg(arg) {
        if (!Object.hasOwn(arg, 'value')) {
            return arg.type;
        }
        switch (arg.type) {
            case 'string':
            case 'number':
            case 'boolean':
            case 'bigint':
                return String(arg.value);
            case 'regexp':
                return `/${arg.value.pattern}/${arg.value.flags ?? ''}`;
            case 'date':
                return new Date(arg.value).toString();
            case 'object':
                return `Object(${arg.value?.length ?? ''})`;
            case 'array':
                return `Array(${arg.value?.length ?? ''})`;
            case 'map':
                return `Map(${arg.value?.length})`;
            case 'set':
                return `Set(${arg.value?.length})`;
            default:
                return arg.type;
        }
    }
    function getRemoteValuesText(args, formatText) {
        const arg = args[0];
        if (!arg) {
            return '';
        }
        // if args[0] is a format specifier, format the args as output
        if (arg.type === 'string' &&
            isFormatSpecifier(arg.value.toString()) &&
            formatText) {
            return logMessageFormatter(args);
        }
        // if args[0] is not a format specifier, just join the args with \u0020 (unicode 'SPACE')
        return args
            .map((arg) => {
            return stringFromArg(arg);
        })
            .join('\u0020');
    }

    var _a$4;
    /** Converts CDP StackTrace object to BiDi StackTrace object. */
    function getBidiStackTrace(cdpStackTrace) {
        const stackFrames = cdpStackTrace?.callFrames.map((callFrame) => {
            return {
                columnNumber: callFrame.columnNumber,
                functionName: callFrame.functionName,
                lineNumber: callFrame.lineNumber,
                url: callFrame.url,
            };
        });
        return stackFrames ? { callFrames: stackFrames } : undefined;
    }
    function getLogLevel(consoleApiType) {
        if (["error" /* Log.Level.Error */, 'assert'].includes(consoleApiType)) {
            return "error" /* Log.Level.Error */;
        }
        if (["debug" /* Log.Level.Debug */, 'trace'].includes(consoleApiType)) {
            return "debug" /* Log.Level.Debug */;
        }
        if (["warn" /* Log.Level.Warn */, 'warning'].includes(consoleApiType)) {
            return "warn" /* Log.Level.Warn */;
        }
        return "info" /* Log.Level.Info */;
    }
    function getLogMethod(consoleApiType) {
        switch (consoleApiType) {
            case 'warning':
                return 'warn';
            case 'startGroup':
                return 'group';
            case 'startGroupCollapsed':
                return 'groupCollapsed';
            case 'endGroup':
                return 'groupEnd';
        }
        return consoleApiType;
    }
    class LogManager {
        #eventManager;
        #realmStorage;
        #cdpTarget;
        #logger;
        constructor(cdpTarget, realmStorage, eventManager, logger) {
            this.#cdpTarget = cdpTarget;
            this.#realmStorage = realmStorage;
            this.#eventManager = eventManager;
            this.#logger = logger;
        }
        static create(cdpTarget, realmStorage, eventManager, logger) {
            const logManager = new _a$4(cdpTarget, realmStorage, eventManager, logger);
            logManager.#initializeEntryAddedEventListener();
            return logManager;
        }
        /**
         * Heuristic serialization of CDP remote object. If possible, return the BiDi value
         * without deep serialization.
         */
        async #heuristicSerializeArg(arg, realm) {
            switch (arg.type) {
                // TODO: Implement regexp, array, object, map and set heuristics base on
                //  preview.
                case 'undefined':
                    return { type: 'undefined' };
                case 'boolean':
                    return { type: 'boolean', value: arg.value };
                case 'string':
                    return { type: 'string', value: arg.value };
                case 'number':
                    // The value can be either a number or a string like `Infinity` or `-0`.
                    return { type: 'number', value: arg.unserializableValue ?? arg.value };
                case 'bigint':
                    if (arg.unserializableValue !== undefined &&
                        arg.unserializableValue[arg.unserializableValue.length - 1] === 'n') {
                        return {
                            type: arg.type,
                            value: arg.unserializableValue.slice(0, -1),
                        };
                    }
                    // Unexpected bigint value, fall back to CDP deep serialization.
                    break;
                case 'object':
                    if (arg.subtype === 'null') {
                        return { type: 'null' };
                    }
                    // Fall back to CDP deep serialization.
                    break;
            }
            // Fall back to CDP deep serialization.
            return await realm.serializeCdpObject(arg, "none" /* Script.ResultOwnership.None */);
        }
        #initializeEntryAddedEventListener() {
            this.#cdpTarget.cdpClient.on('Runtime.consoleAPICalled', (params) => {
                // Try to find realm by `cdpSessionId` and `executionContextId`,
                // if provided.
                const realm = this.#realmStorage.findRealm({
                    cdpSessionId: this.#cdpTarget.cdpSessionId,
                    executionContextId: params.executionContextId,
                });
                if (realm === undefined) {
                    // Ignore exceptions not attached to any realm.
                    this.#logger?.(LogType.cdp, params);
                    return;
                }
                const argsPromise = Promise.all(params.args.map((arg) => this.#heuristicSerializeArg(arg, realm)));
                for (const browsingContext of realm.associatedBrowsingContexts) {
                    this.#eventManager.registerPromiseEvent(argsPromise.then((args) => ({
                        kind: 'success',
                        value: {
                            type: 'event',
                            method: Log$1.EventNames.LogEntryAdded,
                            params: {
                                level: getLogLevel(params.type),
                                source: realm.source,
                                text: getRemoteValuesText(args, true),
                                timestamp: Math.round(params.timestamp),
                                stackTrace: getBidiStackTrace(params.stackTrace),
                                type: 'console',
                                method: getLogMethod(params.type),
                                args,
                            },
                        },
                    }), (error) => ({
                        kind: 'error',
                        error,
                    })), browsingContext.id, Log$1.EventNames.LogEntryAdded);
                }
            });
            this.#cdpTarget.cdpClient.on('Runtime.exceptionThrown', (params) => {
                // Try to find realm by `cdpSessionId` and `executionContextId`,
                // if provided.
                const realm = this.#realmStorage.findRealm({
                    cdpSessionId: this.#cdpTarget.cdpSessionId,
                    executionContextId: params.exceptionDetails.executionContextId,
                });
                if (realm === undefined) {
                    // Ignore exceptions not attached to any realm.
                    this.#logger?.(LogType.cdp, params);
                    return;
                }
                for (const browsingContext of realm.associatedBrowsingContexts) {
                    this.#eventManager.registerPromiseEvent(_a$4.#getExceptionText(params, realm).then((text) => ({
                        kind: 'success',
                        value: {
                            type: 'event',
                            method: Log$1.EventNames.LogEntryAdded,
                            params: {
                                level: "error" /* Log.Level.Error */,
                                source: realm.source,
                                text,
                                timestamp: Math.round(params.timestamp),
                                stackTrace: getBidiStackTrace(params.exceptionDetails.stackTrace),
                                type: 'javascript',
                            },
                        },
                    }), (error) => ({
                        kind: 'error',
                        error,
                    })), browsingContext.id, Log$1.EventNames.LogEntryAdded);
                }
            });
        }
        /**
         * Try the best to get the exception text.
         */
        static async #getExceptionText(params, realm) {
            if (!params.exceptionDetails.exception) {
                return params.exceptionDetails.text;
            }
            if (realm === undefined) {
                return JSON.stringify(params.exceptionDetails.exception);
            }
            return await realm.stringifyObject(params.exceptionDetails.exception);
        }
    }
    _a$4 = LogManager;

    class CdpTarget {
        #id;
        #cdpClient;
        #browserCdpClient;
        #realmStorage;
        #eventManager;
        #preloadScriptStorage;
        #browsingContextStorage;
        #networkStorage;
        #unblocked = new Deferred();
        #unhandledPromptBehavior;
        #logger;
        #deviceAccessEnabled = false;
        #cacheDisableState = false;
        #networkDomainEnabled = false;
        #fetchDomainStages = {
            request: false,
            response: false,
            auth: false,
        };
        static create(targetId, cdpClient, browserCdpClient, realmStorage, eventManager, preloadScriptStorage, browsingContextStorage, networkStorage, unhandledPromptBehavior, logger) {
            const cdpTarget = new CdpTarget(targetId, cdpClient, browserCdpClient, eventManager, realmStorage, preloadScriptStorage, browsingContextStorage, networkStorage, unhandledPromptBehavior, logger);
            LogManager.create(cdpTarget, realmStorage, eventManager, logger);
            cdpTarget.#setEventListeners();
            // No need to await.
            // Deferred will be resolved when the target is unblocked.
            void cdpTarget.#unblock();
            return cdpTarget;
        }
        constructor(targetId, cdpClient, browserCdpClient, eventManager, realmStorage, preloadScriptStorage, browsingContextStorage, networkStorage, unhandledPromptBehavior, logger) {
            this.#id = targetId;
            this.#cdpClient = cdpClient;
            this.#browserCdpClient = browserCdpClient;
            this.#eventManager = eventManager;
            this.#realmStorage = realmStorage;
            this.#preloadScriptStorage = preloadScriptStorage;
            this.#networkStorage = networkStorage;
            this.#browsingContextStorage = browsingContextStorage;
            this.#unhandledPromptBehavior = unhandledPromptBehavior;
            this.#logger = logger;
        }
        /** Returns a deferred that resolves when the target is unblocked. */
        get unblocked() {
            return this.#unblocked;
        }
        get id() {
            return this.#id;
        }
        get cdpClient() {
            return this.#cdpClient;
        }
        get browserCdpClient() {
            return this.#browserCdpClient;
        }
        /** Needed for CDP escape path. */
        get cdpSessionId() {
            // SAFETY we got the client by it's id for creating
            return this.#cdpClient.sessionId;
        }
        /**
         * Enables all the required CDP domains and unblocks the target.
         */
        async #unblock() {
            try {
                await Promise.all([
                    this.#cdpClient.sendCommand('Page.enable'),
                    // There can be some existing frames in the target, if reconnecting to an
                    // existing browser instance, e.g. via Puppeteer. Need to restore the browsing
                    // contexts for the frames to correctly handle further events, like
                    // `Runtime.executionContextCreated`.
                    // It's important to schedule this task together with enabling domains commands to
                    // prepare the tree before the events (e.g. Runtime.executionContextCreated) start
                    // coming.
                    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/2282
                    this.#cdpClient
                        .sendCommand('Page.getFrameTree')
                        .then((frameTree) => this.#restoreFrameTreeState(frameTree.frameTree)),
                    this.#cdpClient.sendCommand('Runtime.enable'),
                    this.#cdpClient.sendCommand('Page.setLifecycleEventsEnabled', {
                        enabled: true,
                    }),
                    this.toggleNetworkIfNeeded(),
                    this.#cdpClient.sendCommand('Target.setAutoAttach', {
                        autoAttach: true,
                        waitForDebuggerOnStart: true,
                        flatten: true,
                    }),
                    this.#initAndEvaluatePreloadScripts(),
                    this.#cdpClient.sendCommand('Runtime.runIfWaitingForDebugger'),
                    this.toggleDeviceAccessIfNeeded(),
                ]);
            }
            catch (error) {
                this.#logger?.(LogType.debugError, 'Failed to unblock target', error);
                // The target might have been closed before the initialization finished.
                if (!this.#cdpClient.isCloseError(error)) {
                    this.#unblocked.resolve({
                        kind: 'error',
                        error,
                    });
                    return;
                }
            }
            this.#unblocked.resolve({
                kind: 'success',
                value: undefined,
            });
        }
        #restoreFrameTreeState(frameTree) {
            const frame = frameTree.frame;
            const maybeContext = this.#browsingContextStorage.findContext(frame.id);
            if (maybeContext !== undefined) {
                // Restoring parent of already known browsing context. This means the target is
                // OOPiF and the BiDi session was connected to already existing browser instance.
                if (maybeContext.parentId === null &&
                    frame.parentId !== null &&
                    frame.parentId !== undefined) {
                    maybeContext.parentId = frame.parentId;
                }
            }
            if (maybeContext === undefined && frame.parentId !== undefined) {
                // Restore not yet known nested frames. The top-level frame is created when the
                // target is attached.
                const parentBrowsingContext = this.#browsingContextStorage.getContext(frame.parentId);
                BrowsingContextImpl.create(frame.id, frame.parentId, parentBrowsingContext.userContext, parentBrowsingContext.cdpTarget, this.#eventManager, this.#browsingContextStorage, this.#realmStorage, frame.url, undefined, this.#unhandledPromptBehavior, this.#logger);
            }
            frameTree.childFrames?.map((frameTree) => this.#restoreFrameTreeState(frameTree));
        }
        async toggleFetchIfNeeded() {
            const stages = this.#networkStorage.getInterceptionStages(this.topLevelId);
            if (
            // Only toggle interception when Network is enabled
            !this.#networkDomainEnabled ||
                (this.#fetchDomainStages.request === stages.request &&
                    this.#fetchDomainStages.response === stages.response &&
                    this.#fetchDomainStages.auth === stages.auth)) {
                return;
            }
            const patterns = [];
            this.#fetchDomainStages = stages;
            if (stages.request || stages.auth) {
                // CDP quirk we need request interception when we intercept auth
                patterns.push({
                    urlPattern: '*',
                    requestStage: 'Request',
                });
            }
            if (stages.response) {
                patterns.push({
                    urlPattern: '*',
                    requestStage: 'Response',
                });
            }
            if (patterns.length) {
                await this.#cdpClient.sendCommand('Fetch.enable', {
                    patterns,
                    handleAuthRequests: stages.auth,
                });
            }
            else {
                await this.#cdpClient.sendCommand('Fetch.disable');
            }
        }
        /**
         * Toggles both Network and Fetch domains.
         */
        async toggleNetworkIfNeeded() {
            const enabled = this.isSubscribedTo(BiDiModule.Network);
            if (enabled === this.#networkDomainEnabled) {
                return;
            }
            this.#networkDomainEnabled = enabled;
            try {
                await Promise.all([
                    this.#cdpClient
                        .sendCommand(enabled ? 'Network.enable' : 'Network.disable')
                        .then(async () => await this.toggleSetCacheDisabled()),
                    this.toggleFetchIfNeeded(),
                ]);
            }
            catch (err) {
                this.#logger?.(LogType.debugError, err);
                this.#networkDomainEnabled = !enabled;
                if (!this.#isExpectedError(err)) {
                    throw err;
                }
            }
        }
        async toggleSetCacheDisabled(disable) {
            const defaultCacheDisabled = this.#networkStorage.defaultCacheBehavior === 'bypass';
            const cacheDisabled = disable ?? defaultCacheDisabled;
            if (!this.#networkDomainEnabled ||
                this.#cacheDisableState === cacheDisabled) {
                return;
            }
            this.#cacheDisableState = cacheDisabled;
            try {
                await this.#cdpClient.sendCommand('Network.setCacheDisabled', {
                    cacheDisabled,
                });
            }
            catch (err) {
                this.#logger?.(LogType.debugError, err);
                this.#cacheDisableState = !cacheDisabled;
                if (!this.#isExpectedError(err)) {
                    throw err;
                }
            }
        }
        async toggleDeviceAccessIfNeeded() {
            const enabled = this.isSubscribedTo(BiDiModule.Bluetooth);
            if (this.#deviceAccessEnabled === enabled) {
                return;
            }
            this.#deviceAccessEnabled = enabled;
            try {
                await this.#cdpClient.sendCommand(enabled ? 'DeviceAccess.enable' : 'DeviceAccess.disable');
            }
            catch (err) {
                this.#logger?.(LogType.debugError, err);
                this.#deviceAccessEnabled = !enabled;
                if (!this.#isExpectedError(err)) {
                    throw err;
                }
            }
        }
        /**
         * Heuristic checking if the error is due to the session being closed. If so, ignore the
         * error.
         */
        #isExpectedError(err) {
            const error = err;
            return (error.code === -32001 &&
                error.message === 'Session with given id not found.');
        }
        #setEventListeners() {
            this.#cdpClient.on('*', (event, params) => {
                // We may encounter uses for EventEmitter other than CDP events,
                // which we want to skip.
                if (typeof event !== 'string') {
                    return;
                }
                this.#eventManager.registerEvent({
                    type: 'event',
                    method: `cdp.${event}`,
                    params: {
                        event,
                        params,
                        session: this.cdpSessionId,
                    },
                }, this.id);
            });
        }
        /**
         * All the ProxyChannels from all the preload scripts of the given
         * BrowsingContext.
         */
        getChannels() {
            return this.#preloadScriptStorage
                .find()
                .flatMap((script) => script.channels);
        }
        /** Loads all top-level preload scripts. */
        async #initAndEvaluatePreloadScripts() {
            await Promise.all(this.#preloadScriptStorage
                .find({
                // Needed for OOPIF
                targetId: this.topLevelId,
                global: true,
            })
                .map((script) => {
                return script.initInTarget(this, true);
            }));
        }
        get topLevelId() {
            return (this.#browsingContextStorage.findTopLevelContextId(this.id) ?? this.id);
        }
        isSubscribedTo(moduleOrEvent) {
            return this.#eventManager.subscriptionManager.isSubscribedTo(moduleOrEvent, this.topLevelId);
        }
    }

    const cdpToBidiTargetTypes = {
        service_worker: 'service-worker',
        shared_worker: 'shared-worker',
        worker: 'dedicated-worker',
    };
    class CdpTargetManager {
        #browserCdpClient;
        #cdpConnection;
        #targetKeysToBeIgnoredByAutoAttach = new Set();
        #selfTargetId;
        #eventManager;
        #browsingContextStorage;
        #networkStorage;
        #bluetoothProcessor;
        #preloadScriptStorage;
        #realmStorage;
        #defaultUserContextId;
        #logger;
        #unhandledPromptBehavior;
        constructor(cdpConnection, browserCdpClient, selfTargetId, eventManager, browsingContextStorage, realmStorage, networkStorage, bluetoothProcessor, preloadScriptStorage, defaultUserContextId, unhandledPromptBehavior, logger) {
            this.#cdpConnection = cdpConnection;
            this.#browserCdpClient = browserCdpClient;
            this.#targetKeysToBeIgnoredByAutoAttach.add(selfTargetId);
            this.#selfTargetId = selfTargetId;
            this.#eventManager = eventManager;
            this.#browsingContextStorage = browsingContextStorage;
            this.#preloadScriptStorage = preloadScriptStorage;
            this.#networkStorage = networkStorage;
            this.#bluetoothProcessor = bluetoothProcessor;
            this.#realmStorage = realmStorage;
            this.#defaultUserContextId = defaultUserContextId;
            this.#unhandledPromptBehavior = unhandledPromptBehavior;
            this.#logger = logger;
            this.#setEventListeners(browserCdpClient);
        }
        /**
         * This method is called for each CDP session, since this class is responsible
         * for creating and destroying all targets and browsing contexts.
         */
        #setEventListeners(cdpClient) {
            cdpClient.on('Target.attachedToTarget', (params) => {
                this.#handleAttachedToTargetEvent(params, cdpClient);
            });
            cdpClient.on('Target.detachedFromTarget', this.#handleDetachedFromTargetEvent.bind(this));
            cdpClient.on('Target.targetInfoChanged', this.#handleTargetInfoChangedEvent.bind(this));
            cdpClient.on('Inspector.targetCrashed', () => {
                this.#handleTargetCrashedEvent(cdpClient);
            });
            cdpClient.on('Page.frameAttached', this.#handleFrameAttachedEvent.bind(this));
            cdpClient.on('Page.frameDetached', this.#handleFrameDetachedEvent.bind(this));
            cdpClient.on('Page.frameSubtreeWillBeDetached', this.#handleFrameSubtreeWillBeDetached.bind(this));
        }
        #handleFrameAttachedEvent(params) {
            const parentBrowsingContext = this.#browsingContextStorage.findContext(params.parentFrameId);
            if (parentBrowsingContext !== undefined) {
                BrowsingContextImpl.create(params.frameId, params.parentFrameId, parentBrowsingContext.userContext, parentBrowsingContext.cdpTarget, this.#eventManager, this.#browsingContextStorage, this.#realmStorage, 
                // At this point, we don't know the URL of the frame yet, so it will be updated
                // later.
                'about:blank', undefined, this.#unhandledPromptBehavior, this.#logger);
            }
        }
        #handleFrameDetachedEvent(params) {
            // In case of OOPiF no need in deleting BrowsingContext.
            if (params.reason === 'swap') {
                return;
            }
            this.#browsingContextStorage.findContext(params.frameId)?.dispose(true);
        }
        #handleFrameSubtreeWillBeDetached(params) {
            this.#browsingContextStorage.findContext(params.frameId)?.dispose(true);
        }
        #handleAttachedToTargetEvent(params, parentSessionCdpClient) {
            const { sessionId, targetInfo } = params;
            const targetCdpClient = this.#cdpConnection.getCdpClient(sessionId);
            const detach = async () => {
                // Detaches and resumes the target suppressing errors.
                await targetCdpClient
                    .sendCommand('Runtime.runIfWaitingForDebugger')
                    .then(() => parentSessionCdpClient.sendCommand('Target.detachFromTarget', params))
                    .catch((error) => this.#logger?.(LogType.debugError, error));
            };
            if (this.#selfTargetId !== targetInfo.targetId) {
                // Service workers are special case because they attach to the
                // browser target and the page target (so twice per worker) during
                // the regular auto-attach and might hang if the CDP session on
                // the browser level is not detached. The logic to detach the
                // right session is handled in the switch below.
                const targetKey = targetInfo.type === 'service_worker'
                    ? `${parentSessionCdpClient.sessionId}_${targetInfo.targetId}`
                    : targetInfo.targetId;
                // Mapper generally only needs one session per target. If we
                // receive additional auto-attached sessions, that is very likely
                // coming from custom CDP sessions.
                if (this.#targetKeysToBeIgnoredByAutoAttach.has(targetKey)) {
                    // Return to leave the session untouched.
                    return;
                }
                this.#targetKeysToBeIgnoredByAutoAttach.add(targetKey);
            }
            switch (targetInfo.type) {
                case 'page':
                case 'iframe': {
                    if (this.#selfTargetId === targetInfo.targetId) {
                        void detach();
                        return;
                    }
                    const cdpTarget = this.#createCdpTarget(targetCdpClient, targetInfo);
                    const maybeContext = this.#browsingContextStorage.findContext(targetInfo.targetId);
                    if (maybeContext && targetInfo.type === 'iframe') {
                        // OOPiF.
                        maybeContext.updateCdpTarget(cdpTarget);
                    }
                    else {
                        const userContext = targetInfo.browserContextId &&
                            targetInfo.browserContextId !== this.#defaultUserContextId
                            ? targetInfo.browserContextId
                            : 'default';
                        // New context.
                        BrowsingContextImpl.create(targetInfo.targetId, null, userContext, cdpTarget, this.#eventManager, this.#browsingContextStorage, this.#realmStorage, 
                        // Hack: when a new target created, CDP emits targetInfoChanged with an empty
                        // url, and navigates it to about:blank later. When the event is emitted for
                        // an existing target (reconnect), the url is already known, and navigation
                        // events will not be emitted anymore. Replacing empty url with `about:blank`
                        // allows to handle both cases in the same way.
                        // "7.3.2.1 Creating browsing contexts".
                        // https://html.spec.whatwg.org/multipage/document-sequences.html#creating-browsing-contexts
                        // TODO: check who to deal with non-null creator and its `creatorOrigin`.
                        targetInfo.url === '' ? 'about:blank' : targetInfo.url, targetInfo.openerFrameId ?? targetInfo.openerId, this.#unhandledPromptBehavior, this.#logger);
                    }
                    return;
                }
                case 'service_worker':
                case 'worker': {
                    const realm = this.#realmStorage.findRealm({
                        cdpSessionId: parentSessionCdpClient.sessionId,
                    });
                    // If there is no browsing context, this worker is already terminated.
                    if (!realm) {
                        void detach();
                        return;
                    }
                    const cdpTarget = this.#createCdpTarget(targetCdpClient, targetInfo);
                    this.#handleWorkerTarget(cdpToBidiTargetTypes[targetInfo.type], cdpTarget, realm);
                    return;
                }
                // In CDP, we only emit shared workers on the browser and not the set of
                // frames that use the shared worker. If we change this in the future to
                // behave like service workers (emits on both browser and frame targets),
                // we can remove this block and merge service workers with the above one.
                case 'shared_worker': {
                    const cdpTarget = this.#createCdpTarget(targetCdpClient, targetInfo);
                    this.#handleWorkerTarget(cdpToBidiTargetTypes[targetInfo.type], cdpTarget);
                    return;
                }
            }
            // DevTools or some other not supported by BiDi target. Just release
            // debugger and ignore them.
            void detach();
        }
        #createCdpTarget(targetCdpClient, targetInfo) {
            this.#setEventListeners(targetCdpClient);
            const target = CdpTarget.create(targetInfo.targetId, targetCdpClient, this.#browserCdpClient, this.#realmStorage, this.#eventManager, this.#preloadScriptStorage, this.#browsingContextStorage, this.#networkStorage, this.#unhandledPromptBehavior, this.#logger);
            this.#networkStorage.onCdpTargetCreated(target);
            this.#bluetoothProcessor.onCdpTargetCreated(target);
            return target;
        }
        #workers = new Map();
        #handleWorkerTarget(realmType, cdpTarget, ownerRealm) {
            cdpTarget.cdpClient.on('Runtime.executionContextCreated', (params) => {
                const { uniqueId, id, origin } = params.context;
                const workerRealm = new WorkerRealm(cdpTarget.cdpClient, this.#eventManager, id, this.#logger, serializeOrigin(origin), ownerRealm ? [ownerRealm] : [], uniqueId, this.#realmStorage, realmType);
                this.#workers.set(cdpTarget.cdpSessionId, workerRealm);
            });
        }
        #handleDetachedFromTargetEvent({ sessionId, targetId, }) {
            if (targetId) {
                this.#preloadScriptStorage.find({ targetId }).map((preloadScript) => {
                    preloadScript.dispose(targetId);
                });
            }
            const context = this.#browsingContextStorage.findContextBySession(sessionId);
            if (context) {
                context.dispose(true);
                return;
            }
            const worker = this.#workers.get(sessionId);
            if (worker) {
                this.#realmStorage.deleteRealms({
                    cdpSessionId: worker.cdpClient.sessionId,
                });
            }
        }
        #handleTargetInfoChangedEvent(params) {
            const context = this.#browsingContextStorage.findContext(params.targetInfo.targetId);
            if (context) {
                context.onTargetInfoChanged(params);
            }
        }
        #handleTargetCrashedEvent(cdpClient) {
            // This is primarily used for service and shared workers. CDP tends to not
            // signal they closed gracefully and instead says they crashed to signal
            // they are closed.
            const realms = this.#realmStorage.findRealms({
                cdpSessionId: cdpClient.sessionId,
            });
            for (const realm of realms) {
                realm.dispose();
            }
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** Container class for browsing contexts. */
    class BrowsingContextStorage {
        /** Map from context ID to context implementation. */
        #contexts = new Map();
        /** Gets all top-level contexts, i.e. those with no parent. */
        getTopLevelContexts() {
            return this.getAllContexts().filter((context) => context.isTopLevelContext());
        }
        /** Gets all contexts. */
        getAllContexts() {
            return Array.from(this.#contexts.values());
        }
        /** Deletes the context with the given ID. */
        deleteContextById(id) {
            this.#contexts.delete(id);
        }
        /** Deletes the given context. */
        deleteContext(context) {
            this.#contexts.delete(context.id);
        }
        /** Tracks the given context. */
        addContext(context) {
            this.#contexts.set(context.id, context);
        }
        /** Returns true whether there is an existing context with the given ID. */
        hasContext(id) {
            return this.#contexts.has(id);
        }
        /** Gets the context with the given ID, if any. */
        findContext(id) {
            return this.#contexts.get(id);
        }
        /** Returns the top-level context ID of the given context, if any. */
        findTopLevelContextId(id) {
            if (id === null) {
                return null;
            }
            const maybeContext = this.findContext(id);
            const parentId = maybeContext?.parentId ?? null;
            if (parentId === null) {
                return id;
            }
            return this.findTopLevelContextId(parentId);
        }
        findContextBySession(sessionId) {
            for (const context of this.#contexts.values()) {
                if (context.cdpTarget.cdpSessionId === sessionId) {
                    return context;
                }
            }
            return;
        }
        /** Gets the context with the given ID, if any, otherwise throws. */
        getContext(id) {
            const result = this.findContext(id);
            if (result === undefined) {
                throw new NoSuchFrameException(`Context ${id} not found`);
            }
            return result;
        }
        verifyTopLevelContextsList(contexts) {
            const foundContexts = new Set();
            if (!contexts) {
                return foundContexts;
            }
            for (const contextId of contexts) {
                const context = this.getContext(contextId);
                if (context.isTopLevelContext()) {
                    foundContexts.add(context);
                }
                else {
                    throw new InvalidArgumentException(`Non top-level context '${contextId}' given.`);
                }
            }
            return foundContexts;
        }
    }

    /*
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     *
     */
    var _a$3;
    const REALM_REGEX = /(?<=realm=").*(?=")/;
    /** Abstracts one individual network request. */
    class NetworkRequest {
        static unknownParameter = 'UNKNOWN';
        /**
         * Each network request has an associated request id, which is a string
         * uniquely identifying that request.
         *
         * The identifier for a request resulting from a redirect matches that of the
         * request that initiated it.
         */
        #id;
        #fetchId;
        /**
         * Indicates the network intercept phase, if the request is currently blocked.
         * Undefined necessarily implies that the request is not blocked.
         */
        #interceptPhase;
        #servedFromCache = false;
        #redirectCount;
        #request = {};
        #requestOverrides;
        #responseOverrides;
        #response = {};
        #eventManager;
        #networkStorage;
        #cdpTarget;
        #logger;
        #emittedEvents = {
            [Network$2.EventNames.AuthRequired]: false,
            [Network$2.EventNames.BeforeRequestSent]: false,
            [Network$2.EventNames.FetchError]: false,
            [Network$2.EventNames.ResponseCompleted]: false,
            [Network$2.EventNames.ResponseStarted]: false,
        };
        waitNextPhase = new Deferred();
        constructor(id, eventManager, networkStorage, cdpTarget, redirectCount = 0, logger) {
            this.#id = id;
            this.#eventManager = eventManager;
            this.#networkStorage = networkStorage;
            this.#cdpTarget = cdpTarget;
            this.#redirectCount = redirectCount;
            this.#logger = logger;
        }
        get id() {
            return this.#id;
        }
        get fetchId() {
            return this.#fetchId;
        }
        /**
         * When blocked returns the phase for it
         */
        get interceptPhase() {
            return this.#interceptPhase;
        }
        get url() {
            const fragment = this.#request.info?.request.urlFragment ??
                this.#request.paused?.request.urlFragment ??
                '';
            const url = this.#response.info?.url ??
                this.#response.paused?.request.url ??
                this.#requestOverrides?.url ??
                this.#request.auth?.request.url ??
                this.#request.info?.request.url ??
                this.#request.paused?.request.url ??
                _a$3.unknownParameter;
            return `${url}${fragment}`;
        }
        get redirectCount() {
            return this.#redirectCount;
        }
        get cdpTarget() {
            return this.#cdpTarget;
        }
        get cdpClient() {
            return this.#cdpTarget.cdpClient;
        }
        isRedirecting() {
            return Boolean(this.#request.info);
        }
        #isDataUrl() {
            return this.url.startsWith('data:');
        }
        get #method() {
            return (this.#requestOverrides?.method ??
                this.#request.info?.request.method ??
                this.#request.paused?.request.method ??
                this.#request.auth?.request.method ??
                this.#response.paused?.request.method);
        }
        get #navigationId() {
            // Heuristic to determine if this is a navigation request, and if not return null.
            if (!this.#request.info ||
                !this.#request.info.loaderId ||
                // When we navigate all CDP network events have `loaderId`
                // CDP's `loaderId` and `requestId` match when
                // that request triggered the loading
                this.#request.info.loaderId !== this.#request.info.requestId) {
                return null;
            }
            // Get virtual navigation ID from the browsing context.
            return this.#networkStorage.getNavigationId(this.#context ?? undefined);
        }
        get #cookies() {
            let cookies = [];
            if (this.#request.extraInfo) {
                cookies = this.#request.extraInfo.associatedCookies
                    .filter(({ blockedReasons }) => {
                    return !Array.isArray(blockedReasons) || blockedReasons.length === 0;
                })
                    .map(({ cookie }) => cdpToBiDiCookie(cookie));
            }
            return cookies;
        }
        get #bodySize() {
            let bodySize = 0;
            if (typeof this.#requestOverrides?.bodySize === 'number') {
                bodySize = this.#requestOverrides.bodySize;
            }
            else {
                bodySize = bidiBodySizeFromCdpPostDataEntries(this.#request.info?.request.postDataEntries ?? []);
            }
            return bodySize;
        }
        get #context() {
            return (this.#response.paused?.frameId ??
                this.#request.info?.frameId ??
                this.#request.paused?.frameId ??
                this.#request.auth?.frameId ??
                null);
        }
        /** Returns the HTTP status code associated with this request if any. */
        get #statusCode() {
            return (this.#responseOverrides?.statusCode ??
                this.#response.paused?.responseStatusCode ??
                this.#response.extraInfo?.statusCode ??
                this.#response.info?.status);
        }
        get #requestHeaders() {
            let headers = [];
            if (this.#requestOverrides?.headers) {
                headers = this.#requestOverrides.headers;
            }
            else {
                headers = [
                    ...bidiNetworkHeadersFromCdpNetworkHeaders(this.#request.info?.request.headers),
                    ...bidiNetworkHeadersFromCdpNetworkHeaders(this.#request.extraInfo?.headers),
                ];
            }
            return headers;
        }
        get #authChallenges() {
            // TODO: get headers from Fetch.requestPaused
            if (!this.#response.info) {
                return;
            }
            if (!(this.#statusCode === 401 || this.#statusCode === 407)) {
                return undefined;
            }
            const headerName = this.#statusCode === 401 ? 'WWW-Authenticate' : 'Proxy-Authenticate';
            const authChallenges = [];
            for (const [header, value] of Object.entries(this.#response.info.headers)) {
                // TODO: Do a proper match based on https://httpwg.org/specs/rfc9110.html#credentials
                // Or verify this works
                if (header.localeCompare(headerName, undefined, { sensitivity: 'base' }) === 0) {
                    authChallenges.push({
                        scheme: value.split(' ').at(0) ?? '',
                        realm: value.match(REALM_REGEX)?.at(0) ?? '',
                    });
                }
            }
            return authChallenges;
        }
        get #timings() {
            return {
                // TODO: Verify this is correct
                timeOrigin: getTiming(this.#response.info?.timing?.requestTime),
                requestTime: getTiming(this.#response.info?.timing?.requestTime),
                redirectStart: 0,
                redirectEnd: 0,
                // TODO: Verify this is correct
                // https://source.chromium.org/chromium/chromium/src/+/main:net/base/load_timing_info.h;l=145
                fetchStart: getTiming(this.#response.info?.timing?.requestTime),
                dnsStart: getTiming(this.#response.info?.timing?.dnsStart),
                dnsEnd: getTiming(this.#response.info?.timing?.dnsEnd),
                connectStart: getTiming(this.#response.info?.timing?.connectStart),
                connectEnd: getTiming(this.#response.info?.timing?.connectEnd),
                tlsStart: getTiming(this.#response.info?.timing?.sslStart),
                requestStart: getTiming(this.#response.info?.timing?.sendStart),
                // https://source.chromium.org/chromium/chromium/src/+/main:net/base/load_timing_info.h;l=196
                responseStart: getTiming(this.#response.info?.timing?.receiveHeadersStart),
                responseEnd: getTiming(this.#response.info?.timing?.receiveHeadersEnd),
            };
        }
        #phaseChanged() {
            this.waitNextPhase.resolve();
            this.waitNextPhase = new Deferred();
        }
        #interceptsInPhase(phase) {
            if (!this.#cdpTarget.isSubscribedTo(`network.${phase}`)) {
                return new Set();
            }
            return this.#networkStorage.getInterceptsForPhase(this, phase);
        }
        #isBlockedInPhase(phase) {
            return this.#interceptsInPhase(phase).size > 0;
        }
        handleRedirect(event) {
            // TODO: use event.redirectResponse;
            // Temporary workaround to emit ResponseCompleted event for redirects
            this.#response.hasExtraInfo = false;
            this.#response.info = event.redirectResponse;
            this.#emitEventsIfReady({
                wasRedirected: true,
            });
        }
        #emitEventsIfReady(options = {}) {
            const requestExtraInfoCompleted = 
            // Flush redirects
            options.wasRedirected ||
                options.hasFailed ||
                this.#isDataUrl() ||
                Boolean(this.#request.extraInfo) ||
                // Requests from cache don't have extra info
                this.#servedFromCache ||
                // Sometimes there is no extra info and the response
                // is the only place we can find out
                Boolean(this.#response.info && !this.#response.hasExtraInfo);
            const noInterceptionExpected = 
            // We can't intercept data urls from CDP
            this.#isDataUrl() ||
                // Cached requests never hit the network
                this.#servedFromCache;
            const requestInterceptionExpected = !noInterceptionExpected &&
                this.#isBlockedInPhase("beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */);
            const requestInterceptionCompleted = !requestInterceptionExpected ||
                (requestInterceptionExpected && Boolean(this.#request.paused));
            if (Boolean(this.#request.info) &&
                (requestInterceptionExpected
                    ? requestInterceptionCompleted
                    : requestExtraInfoCompleted)) {
                this.#emitEvent(this.#getBeforeRequestEvent.bind(this));
            }
            const responseExtraInfoCompleted = Boolean(this.#response.extraInfo) ||
                // Response from cache don't have extra info
                this.#servedFromCache ||
                // Don't expect extra info if the flag is false
                Boolean(this.#response.info && !this.#response.hasExtraInfo);
            const responseInterceptionExpected = !noInterceptionExpected &&
                this.#isBlockedInPhase("responseStarted" /* Network.InterceptPhase.ResponseStarted */);
            if (this.#response.info ||
                (responseInterceptionExpected && Boolean(this.#response.paused))) {
                this.#emitEvent(this.#getResponseStartedEvent.bind(this));
            }
            const responseInterceptionCompleted = !responseInterceptionExpected ||
                (responseInterceptionExpected && Boolean(this.#response.paused));
            if (Boolean(this.#response.info) &&
                responseExtraInfoCompleted &&
                responseInterceptionCompleted) {
                this.#emitEvent(this.#getResponseReceivedEvent.bind(this));
                this.#networkStorage.deleteRequest(this.id);
            }
        }
        onRequestWillBeSentEvent(event) {
            this.#request.info = event;
            this.#emitEventsIfReady();
        }
        onRequestWillBeSentExtraInfoEvent(event) {
            this.#request.extraInfo = event;
            this.#emitEventsIfReady();
        }
        onResponseReceivedExtraInfoEvent(event) {
            if (event.statusCode >= 300 &&
                event.statusCode <= 399 &&
                this.#request.info &&
                event.headers['location'] === this.#request.info.request.url) {
                // We received the Response Extra info for the redirect
                // Too late so we need to skip it as it will
                // fire wrongly for the last one
                return;
            }
            this.#response.extraInfo = event;
            this.#emitEventsIfReady();
        }
        onResponseReceivedEvent(event) {
            this.#response.hasExtraInfo = event.hasExtraInfo;
            this.#response.info = event.response;
            this.#emitEventsIfReady();
        }
        onServedFromCache() {
            this.#servedFromCache = true;
            this.#emitEventsIfReady();
        }
        onLoadingFailedEvent(event) {
            this.#emitEventsIfReady({
                hasFailed: true,
            });
            this.#emitEvent(() => {
                return {
                    method: Network$2.EventNames.FetchError,
                    params: {
                        ...this.#getBaseEventParams(),
                        errorText: event.errorText,
                    },
                };
            });
        }
        /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-failRequest */
        async failRequest(errorReason) {
            assert(this.#fetchId, 'Network Interception not set-up.');
            await this.cdpClient.sendCommand('Fetch.failRequest', {
                requestId: this.#fetchId,
                errorReason,
            });
            this.#interceptPhase = undefined;
        }
        onRequestPaused(event) {
            this.#fetchId = event.requestId;
            // CDP https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#event-requestPaused
            if (event.responseStatusCode || event.responseErrorReason) {
                this.#response.paused = event;
                if (this.#isBlockedInPhase("responseStarted" /* Network.InterceptPhase.ResponseStarted */) &&
                    // CDP may emit multiple events for a single request
                    !this.#emittedEvents[Network$2.EventNames.ResponseStarted] &&
                    // Continue all response that have not enabled Network domain
                    this.#fetchId !== this.id) {
                    this.#interceptPhase = "responseStarted" /* Network.InterceptPhase.ResponseStarted */;
                }
                else {
                    void this.#continueResponse();
                }
            }
            else {
                this.#request.paused = event;
                if (this.#isBlockedInPhase("beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */) &&
                    // CDP may emit multiple events for a single request
                    !this.#emittedEvents[Network$2.EventNames.BeforeRequestSent] &&
                    // Continue all requests that have not enabled Network domain
                    this.#fetchId !== this.id) {
                    this.#interceptPhase = "beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */;
                }
                else {
                    void this.#continueRequest();
                }
            }
            this.#emitEventsIfReady();
        }
        onAuthRequired(event) {
            this.#fetchId = event.requestId;
            this.#request.auth = event;
            if (this.#isBlockedInPhase("authRequired" /* Network.InterceptPhase.AuthRequired */) &&
                // Continue all auth requests that have not enabled Network domain
                this.#fetchId !== this.id) {
                this.#interceptPhase = "authRequired" /* Network.InterceptPhase.AuthRequired */;
            }
            else {
                void this.#continueWithAuth({
                    response: 'Default',
                });
            }
            this.#emitEvent(() => {
                return {
                    method: Network$2.EventNames.AuthRequired,
                    params: {
                        ...this.#getBaseEventParams("authRequired" /* Network.InterceptPhase.AuthRequired */),
                        response: this.#getResponseEventParams(),
                    },
                };
            });
        }
        /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-continueRequest */
        async continueRequest(overrides = {}) {
            const overrideHeaders = this.#getOverrideHeader(overrides.headers, overrides.cookies);
            const headers = cdpFetchHeadersFromBidiNetworkHeaders(overrideHeaders);
            const postData = getCdpBodyFromBiDiBytesValue(overrides.body);
            await this.#continueRequest({
                url: overrides.url,
                method: overrides.method,
                headers,
                postData,
            });
            this.#requestOverrides = {
                url: overrides.url,
                method: overrides.method,
                headers: overrides.headers,
                cookies: overrides.cookies,
                bodySize: getSizeFromBiDiBytesValue(overrides.body),
            };
        }
        async #continueRequest(overrides = {}) {
            assert(this.#fetchId, 'Network Interception not set-up.');
            await this.cdpClient.sendCommand('Fetch.continueRequest', {
                requestId: this.#fetchId,
                url: overrides.url,
                method: overrides.method,
                headers: overrides.headers,
                postData: overrides.postData,
            });
            this.#interceptPhase = undefined;
        }
        /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-continueResponse */
        async continueResponse(overrides = {}) {
            if (this.interceptPhase === "authRequired" /* Network.InterceptPhase.AuthRequired */) {
                if (overrides.credentials) {
                    await Promise.all([
                        this.waitNextPhase,
                        await this.#continueWithAuth({
                            response: 'ProvideCredentials',
                            username: overrides.credentials.username,
                            password: overrides.credentials.password,
                        }),
                    ]);
                }
                else {
                    // We need to use `ProvideCredentials`
                    // As `Default` may cancel the request
                    return await this.#continueWithAuth({
                        response: 'ProvideCredentials',
                    });
                }
            }
            if (this.#interceptPhase === "responseStarted" /* Network.InterceptPhase.ResponseStarted */) {
                const overrideHeaders = this.#getOverrideHeader(overrides.headers, overrides.cookies);
                const responseHeaders = cdpFetchHeadersFromBidiNetworkHeaders(overrideHeaders);
                await this.#continueResponse({
                    responseCode: overrides.statusCode ?? this.#response.paused?.responseStatusCode,
                    responsePhrase: overrides.reasonPhrase ?? this.#response.paused?.responseStatusText,
                    responseHeaders: responseHeaders ?? this.#response.paused?.responseHeaders,
                });
                this.#responseOverrides = {
                    statusCode: overrides.statusCode,
                    headers: overrideHeaders,
                };
            }
        }
        async #continueResponse({ responseCode, responsePhrase, responseHeaders, } = {}) {
            assert(this.#fetchId, 'Network Interception not set-up.');
            await this.cdpClient.sendCommand('Fetch.continueResponse', {
                requestId: this.#fetchId,
                responseCode,
                responsePhrase,
                responseHeaders,
            });
            this.#interceptPhase = undefined;
        }
        /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-continueWithAuth */
        async continueWithAuth(authChallenge) {
            let username;
            let password;
            if (authChallenge.action === 'provideCredentials') {
                const { credentials } = authChallenge;
                username = credentials.username;
                password = credentials.password;
            }
            const response = cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction(authChallenge.action);
            await this.#continueWithAuth({
                response,
                username,
                password,
            });
        }
        /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-provideResponse */
        async provideResponse(overrides) {
            assert(this.#fetchId, 'Network Interception not set-up.');
            // We need to pass through if the request is already in
            // AuthRequired phase
            if (this.interceptPhase === "authRequired" /* Network.InterceptPhase.AuthRequired */) {
                // We need to use `ProvideCredentials`
                // As `Default` may cancel the request
                return await this.#continueWithAuth({
                    response: 'ProvideCredentials',
                });
            }
            // If we don't modify the response
            // just continue the request
            if (!overrides.body && !overrides.headers) {
                return await this.#continueRequest();
            }
            const overrideHeaders = this.#getOverrideHeader(overrides.headers, overrides.cookies);
            const responseHeaders = cdpFetchHeadersFromBidiNetworkHeaders(overrideHeaders);
            const responseCode = overrides.statusCode ?? this.#statusCode ?? 200;
            await this.cdpClient.sendCommand('Fetch.fulfillRequest', {
                requestId: this.#fetchId,
                responseCode,
                responsePhrase: overrides.reasonPhrase,
                responseHeaders,
                body: getCdpBodyFromBiDiBytesValue(overrides.body),
            });
            this.#interceptPhase = undefined;
        }
        async #continueWithAuth(authChallengeResponse) {
            assert(this.#fetchId, 'Network Interception not set-up.');
            await this.cdpClient.sendCommand('Fetch.continueWithAuth', {
                requestId: this.#fetchId,
                authChallengeResponse,
            });
            this.#interceptPhase = undefined;
        }
        #emitEvent(getEvent) {
            let event;
            try {
                event = getEvent();
            }
            catch (error) {
                this.#logger?.(LogType.debugError, error);
                return;
            }
            if (this.#isIgnoredEvent() ||
                (this.#emittedEvents[event.method] &&
                    // Special case this event can be emitted multiple times
                    event.method !== Network$2.EventNames.AuthRequired)) {
                return;
            }
            this.#phaseChanged();
            this.#emittedEvents[event.method] = true;
            this.#eventManager.registerEvent(Object.assign(event, {
                type: 'event',
            }), this.#context);
        }
        #getBaseEventParams(phase) {
            const interceptProps = {
                isBlocked: false,
            };
            if (phase) {
                const blockedBy = this.#interceptsInPhase(phase);
                interceptProps.isBlocked = blockedBy.size > 0;
                if (interceptProps.isBlocked) {
                    interceptProps.intercepts = [...blockedBy];
                }
            }
            return {
                context: this.#context,
                navigation: this.#navigationId,
                redirectCount: this.#redirectCount,
                request: this.#getRequestData(),
                // Timestamp should be in milliseconds, while CDP provides it in seconds.
                timestamp: Math.round(getTiming(this.#request.info?.wallTime) * 1000),
                // Contains isBlocked and intercepts
                ...interceptProps,
            };
        }
        #getResponseEventParams() {
            // Chromium sends wrong extraInfo events for responses served from cache.
            // See https://github.com/puppeteer/puppeteer/issues/9965 and
            // https://crbug.com/1340398.
            if (this.#response.info?.fromDiskCache) {
                this.#response.extraInfo = undefined;
            }
            const headers = [
                ...bidiNetworkHeadersFromCdpNetworkHeaders(this.#response.info?.headers),
                ...bidiNetworkHeadersFromCdpNetworkHeaders(this.#response.extraInfo?.headers),
                // TODO: Verify how to dedupe these
                // ...bidiNetworkHeadersFromCdpNetworkHeadersEntries(
                //   this.#response.paused?.responseHeaders
                // ),
            ];
            const authChallenges = this.#authChallenges;
            return {
                url: this.url,
                protocol: this.#response.info?.protocol ?? '',
                status: this.#statusCode ?? -1, // TODO: Throw an exception or use some other status code?
                statusText: this.#response.info?.statusText ||
                    this.#response.paused?.responseStatusText ||
                    '',
                fromCache: this.#response.info?.fromDiskCache ||
                    this.#response.info?.fromPrefetchCache ||
                    this.#servedFromCache,
                headers: this.#responseOverrides?.headers ?? headers,
                mimeType: this.#response.info?.mimeType || '',
                bytesReceived: this.#response.info?.encodedDataLength || 0,
                headersSize: computeHeadersSize(headers),
                // TODO: consider removing from spec.
                bodySize: 0,
                content: {
                    // TODO: consider removing from spec.
                    size: 0,
                },
                ...(authChallenges ? { authChallenges } : {}),
                // @ts-expect-error this is a CDP-specific extension.
                'goog:securityDetails': this.#response.info?.securityDetails,
            };
        }
        #getRequestData() {
            const headers = this.#requestHeaders;
            return {
                request: this.#id,
                url: this.url,
                method: this.#method ?? _a$3.unknownParameter,
                headers,
                cookies: this.#cookies,
                headersSize: computeHeadersSize(headers),
                bodySize: this.#bodySize,
                timings: this.#timings,
                // @ts-expect-error CDP-specific attribute.
                'goog:postData': this.#request.info?.request?.postData,
                'goog:hasPostData': this.#request.info?.request?.hasPostData,
                'goog:resourceType': this.#request.info?.type,
            };
        }
        #getBeforeRequestEvent() {
            assert(this.#request.info, 'RequestWillBeSentEvent is not set');
            return {
                method: Network$2.EventNames.BeforeRequestSent,
                params: {
                    ...this.#getBaseEventParams("beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */),
                    initiator: {
                        type: _a$3.#getInitiatorType(this.#request.info.initiator.type),
                        columnNumber: this.#request.info.initiator.columnNumber,
                        lineNumber: this.#request.info.initiator.lineNumber,
                        stackTrace: this.#request.info.initiator.stack,
                        request: this.#request.info.initiator.requestId,
                    },
                },
            };
        }
        #getResponseStartedEvent() {
            return {
                method: Network$2.EventNames.ResponseStarted,
                params: {
                    ...this.#getBaseEventParams("responseStarted" /* Network.InterceptPhase.ResponseStarted */),
                    response: this.#getResponseEventParams(),
                },
            };
        }
        #getResponseReceivedEvent() {
            return {
                method: Network$2.EventNames.ResponseCompleted,
                params: {
                    ...this.#getBaseEventParams(),
                    response: this.#getResponseEventParams(),
                },
            };
        }
        #isIgnoredEvent() {
            const faviconUrl = '/favicon.ico';
            return (this.#request.paused?.request.url.endsWith(faviconUrl) ??
                this.#request.info?.request.url.endsWith(faviconUrl) ??
                false);
        }
        #getOverrideHeader(headers, cookies) {
            if (!headers && !cookies) {
                return undefined;
            }
            let overrideHeaders = headers;
            const cookieHeader = networkHeaderFromCookieHeaders(cookies);
            if (cookieHeader && !overrideHeaders) {
                overrideHeaders = this.#requestHeaders;
            }
            if (cookieHeader && overrideHeaders) {
                overrideHeaders.filter((header) => header.name.localeCompare('cookie', undefined, {
                    sensitivity: 'base',
                }) !== 0);
                overrideHeaders.push(cookieHeader);
            }
            return overrideHeaders;
        }
        static #getInitiatorType(initiatorType) {
            switch (initiatorType) {
                case 'parser':
                case 'script':
                case 'preflight':
                    return initiatorType;
                default:
                    return 'other';
            }
        }
    }
    _a$3 = NetworkRequest;
    function getCdpBodyFromBiDiBytesValue(body) {
        let parsedBody;
        if (body?.type === 'string') {
            parsedBody = btoa(body.value);
        }
        else if (body?.type === 'base64') {
            parsedBody = body.value;
        }
        return parsedBody;
    }
    function getSizeFromBiDiBytesValue(body) {
        if (body?.type === 'string') {
            return body.value.length;
        }
        else if (body?.type === 'base64') {
            return atob(body.value).length;
        }
        return 0;
    }

    /** Stores network and intercept maps. */
    class NetworkStorage {
        #browsingContextStorage;
        #eventManager;
        #logger;
        /**
         * A map from network request ID to Network Request objects.
         * Needed as long as information about requests comes from different events.
         */
        #requests = new Map();
        /** A map from intercept ID to track active network intercepts. */
        #intercepts = new Map();
        #defaultCacheBehavior = 'default';
        constructor(eventManager, browsingContextStorage, browserClient, logger) {
            this.#browsingContextStorage = browsingContextStorage;
            this.#eventManager = eventManager;
            browserClient.on('Target.detachedFromTarget', ({ sessionId }) => {
                this.disposeRequestMap(sessionId);
            });
            this.#logger = logger;
        }
        /**
         * Gets the network request with the given ID, if any.
         * Otherwise, creates a new network request with the given ID and cdp target.
         */
        #getOrCreateNetworkRequest(id, cdpTarget, redirectCount) {
            let request = this.getRequestById(id);
            if (request) {
                return request;
            }
            request = new NetworkRequest(id, this.#eventManager, this, cdpTarget, redirectCount, this.#logger);
            this.addRequest(request);
            return request;
        }
        onCdpTargetCreated(cdpTarget) {
            const cdpClient = cdpTarget.cdpClient;
            // TODO: Wrap into object
            const listeners = [
                [
                    'Network.requestWillBeSent',
                    (params) => {
                        const request = this.getRequestById(params.requestId);
                        if (request && request.isRedirecting()) {
                            request.handleRedirect(params);
                            this.deleteRequest(params.requestId);
                            this.#getOrCreateNetworkRequest(params.requestId, cdpTarget, request.redirectCount + 1).onRequestWillBeSentEvent(params);
                        }
                        else {
                            this.#getOrCreateNetworkRequest(params.requestId, cdpTarget).onRequestWillBeSentEvent(params);
                        }
                    },
                ],
                [
                    'Network.requestWillBeSentExtraInfo',
                    (params) => {
                        this.#getOrCreateNetworkRequest(params.requestId, cdpTarget).onRequestWillBeSentExtraInfoEvent(params);
                    },
                ],
                [
                    'Network.responseReceived',
                    (params) => {
                        this.#getOrCreateNetworkRequest(params.requestId, cdpTarget).onResponseReceivedEvent(params);
                    },
                ],
                [
                    'Network.responseReceivedExtraInfo',
                    (params) => {
                        this.#getOrCreateNetworkRequest(params.requestId, cdpTarget).onResponseReceivedExtraInfoEvent(params);
                    },
                ],
                [
                    'Network.requestServedFromCache',
                    (params) => {
                        this.#getOrCreateNetworkRequest(params.requestId, cdpTarget).onServedFromCache();
                    },
                ],
                [
                    'Network.loadingFailed',
                    (params) => {
                        this.#getOrCreateNetworkRequest(params.requestId, cdpTarget).onLoadingFailedEvent(params);
                    },
                ],
                [
                    'Fetch.requestPaused',
                    (event) => {
                        this.#getOrCreateNetworkRequest(
                        // CDP quirk if the Network domain is not present this is undefined
                        event.networkId ?? event.requestId, cdpTarget).onRequestPaused(event);
                    },
                ],
                [
                    'Fetch.authRequired',
                    (event) => {
                        let request = this.getRequestByFetchId(event.requestId);
                        if (!request) {
                            request = this.#getOrCreateNetworkRequest(event.requestId, cdpTarget);
                        }
                        request.onAuthRequired(event);
                    },
                ],
            ];
            for (const [event, listener] of listeners) {
                cdpClient.on(event, listener);
            }
        }
        getInterceptionStages(browsingContextId) {
            const stages = {
                request: false,
                response: false,
                auth: false,
            };
            for (const intercept of this.#intercepts.values()) {
                if (intercept.contexts &&
                    !intercept.contexts.includes(browsingContextId)) {
                    continue;
                }
                stages.request ||= intercept.phases.includes("beforeRequestSent" /* Network.InterceptPhase.BeforeRequestSent */);
                stages.response ||= intercept.phases.includes("responseStarted" /* Network.InterceptPhase.ResponseStarted */);
                stages.auth ||= intercept.phases.includes("authRequired" /* Network.InterceptPhase.AuthRequired */);
            }
            return stages;
        }
        getInterceptsForPhase(request, phase) {
            if (request.url === NetworkRequest.unknownParameter) {
                return new Set();
            }
            const intercepts = new Set();
            for (const [interceptId, intercept] of this.#intercepts.entries()) {
                if (!intercept.phases.includes(phase) ||
                    (intercept.contexts &&
                        !intercept.contexts.includes(request.cdpTarget.topLevelId))) {
                    continue;
                }
                if (intercept.urlPatterns.length === 0) {
                    intercepts.add(interceptId);
                    continue;
                }
                for (const pattern of intercept.urlPatterns) {
                    if (matchUrlPattern(pattern, request.url)) {
                        intercepts.add(interceptId);
                        break;
                    }
                }
            }
            return intercepts;
        }
        disposeRequestMap(sessionId) {
            for (const request of this.#requests.values()) {
                if (request.cdpClient.sessionId === sessionId) {
                    this.#requests.delete(request.id);
                }
            }
        }
        /**
         * Adds the given entry to the intercept map.
         * URL patterns are assumed to be parsed.
         *
         * @return The intercept ID.
         */
        addIntercept(value) {
            const interceptId = uuidv4();
            this.#intercepts.set(interceptId, value);
            return interceptId;
        }
        /**
         * Removes the given intercept from the intercept map.
         * Throws NoSuchInterceptException if the intercept does not exist.
         */
        removeIntercept(intercept) {
            if (!this.#intercepts.has(intercept)) {
                throw new NoSuchInterceptException(`Intercept '${intercept}' does not exist.`);
            }
            this.#intercepts.delete(intercept);
        }
        getRequestById(id) {
            return this.#requests.get(id);
        }
        getRequestByFetchId(fetchId) {
            for (const request of this.#requests.values()) {
                if (request.fetchId === fetchId) {
                    return request;
                }
            }
            return;
        }
        addRequest(request) {
            this.#requests.set(request.id, request);
        }
        deleteRequest(id) {
            this.#requests.delete(id);
        }
        /**
         * Gets the virtual navigation ID for the given navigable ID.
         */
        getNavigationId(contextId) {
            if (contextId === undefined) {
                return null;
            }
            return (this.#browsingContextStorage.findContext(contextId)?.navigationId ?? null);
        }
        set defaultCacheBehavior(behavior) {
            this.#defaultCacheBehavior = behavior;
        }
        get defaultCacheBehavior() {
            return this.#defaultCacheBehavior;
        }
    }

    /**
     * Container class for preload scripts.
     */
    class PreloadScriptStorage {
        /** Tracks all BiDi preload scripts.  */
        #scripts = new Set();
        /**
         * Finds all entries that match the given filter (OR logic).
         */
        find(filter) {
            if (!filter) {
                return [...this.#scripts];
            }
            return [...this.#scripts].filter((script) => {
                if (filter.id !== undefined && filter.id === script.id) {
                    return true;
                }
                if (filter.targetId !== undefined &&
                    script.targetIds.has(filter.targetId)) {
                    return true;
                }
                if (filter.global !== undefined &&
                    // Global scripts have no contexts
                    ((filter.global && script.contexts === undefined) ||
                        // Non global scripts always have contexts
                        (!filter.global && script.contexts !== undefined))) {
                    return true;
                }
                return false;
            });
        }
        add(preloadScript) {
            this.#scripts.add(preloadScript);
        }
        /** Deletes all BiDi preload script entries that match the given filter. */
        remove(filter) {
            for (const preloadScript of this.find(filter)) {
                this.#scripts.delete(preloadScript);
            }
        }
    }

    /** Container class for browsing realms. */
    class RealmStorage {
        /** Tracks handles and their realms sent to the client. */
        #knownHandlesToRealmMap = new Map();
        /** Map from realm ID to Realm. */
        #realmMap = new Map();
        get knownHandlesToRealmMap() {
            return this.#knownHandlesToRealmMap;
        }
        addRealm(realm) {
            this.#realmMap.set(realm.realmId, realm);
        }
        /** Finds all realms that match the given filter. */
        findRealms(filter) {
            return Array.from(this.#realmMap.values()).filter((realm) => {
                if (filter.realmId !== undefined && filter.realmId !== realm.realmId) {
                    return false;
                }
                if (filter.browsingContextId !== undefined &&
                    !realm.associatedBrowsingContexts
                        .map((browsingContext) => browsingContext.id)
                        .includes(filter.browsingContextId)) {
                    return false;
                }
                if (filter.sandbox !== undefined &&
                    (!(realm instanceof WindowRealm) || filter.sandbox !== realm.sandbox)) {
                    return false;
                }
                if (filter.executionContextId !== undefined &&
                    filter.executionContextId !== realm.executionContextId) {
                    return false;
                }
                if (filter.origin !== undefined && filter.origin !== realm.origin) {
                    return false;
                }
                if (filter.type !== undefined && filter.type !== realm.realmType) {
                    return false;
                }
                if (filter.cdpSessionId !== undefined &&
                    filter.cdpSessionId !== realm.cdpClient.sessionId) {
                    return false;
                }
                return true;
            });
        }
        findRealm(filter) {
            const maybeRealms = this.findRealms(filter);
            if (maybeRealms.length !== 1) {
                return undefined;
            }
            return maybeRealms[0];
        }
        /** Gets the only realm that matches the given filter, if any, otherwise throws. */
        getRealm(filter) {
            const maybeRealm = this.findRealm(filter);
            if (maybeRealm === undefined) {
                throw new NoSuchFrameException(`Realm ${JSON.stringify(filter)} not found`);
            }
            return maybeRealm;
        }
        /** Deletes all realms that match the given filter. */
        deleteRealms(filter) {
            this.findRealms(filter).map((realm) => {
                realm.dispose();
                this.#realmMap.delete(realm.realmId);
                Array.from(this.knownHandlesToRealmMap.entries())
                    .filter(([, r]) => r === realm.realmId)
                    .map(([handle]) => this.knownHandlesToRealmMap.delete(handle));
            });
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** Implements a FIFO buffer with a fixed size. */
    let Buffer$1 = class Buffer {
        #capacity;
        #entries = [];
        #onItemRemoved;
        /**
         * @param capacity The buffer capacity.
         * @param onItemRemoved Delegate called for each removed element.
         */
        constructor(capacity, onItemRemoved) {
            this.#capacity = capacity;
            this.#onItemRemoved = onItemRemoved;
        }
        get() {
            return this.#entries;
        }
        add(value) {
            this.#entries.push(value);
            while (this.#entries.length > this.#capacity) {
                const item = this.#entries.shift();
                if (item !== undefined) {
                    this.#onItemRemoved?.(item);
                }
            }
        }
    };

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * A subclass of Map whose functionality is almost the same as its parent
     * except for the fact that DefaultMap never returns undefined. It provides a
     * default value for keys that do not exist.
     */
    class DefaultMap extends Map {
        /** The default value to return whenever a key is not present in the map. */
        #getDefaultValue;
        constructor(getDefaultValue, entries) {
            super(entries);
            this.#getDefaultValue = getDefaultValue;
        }
        get(key) {
            if (!this.has(key)) {
                this.set(key, this.#getDefaultValue(key));
            }
            return super.get(key);
        }
    }

    /*
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Returns an array of distinct values. Order is not guaranteed.
     * @param values - The values to filter. Should be JSON-serializable.
     * @return - An array of distinct values.
     */
    function distinctValues(values) {
        const map = new Map();
        for (const value of values) {
            map.set(deterministicJSONStringify(value), value);
        }
        return Array.from(map.values());
    }
    /**
     * Returns a stringified version of the object with keys sorted. This is required to
     * ensure that the stringified version of an object is deterministic independent of the
     * order of keys.
     * @param obj
     * @return {string}
     */
    function deterministicJSONStringify(obj) {
        return JSON.stringify(normalizeObject(obj));
    }
    function normalizeObject(obj) {
        if (obj === undefined ||
            obj === null ||
            Array.isArray(obj) ||
            typeof obj !== 'object') {
            return obj;
        }
        // Copy the original object key and values to a new object in sorted order.
        const newObj = {};
        for (const key of Object.keys(obj).sort()) {
            const value = obj[key];
            newObj[key] = normalizeObject(value); // Recursively sort nested objects
        }
        return newObj;
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Creates an object with a positive unique incrementing id.
     */
    class IdWrapper {
        static #counter = 0;
        #id;
        constructor() {
            this.#id = ++IdWrapper.#counter;
        }
        get id() {
            return this.#id;
        }
    }

    /**
     * Copyright 2023 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Returns true if the given event is a CDP event.
     * @see https://chromedevtools.github.io/devtools-protocol/
     */
    function isCdpEvent(name) {
        return (name.split('.').at(0)?.startsWith(BiDiModule.Cdp) ?? false);
    }
    /**
     * Asserts that the given event is known to BiDi or BiDi+, or throws otherwise.
     */
    function assertSupportedEvent(name) {
        if (!EVENT_NAMES.has(name) && !isCdpEvent(name)) {
            throw new InvalidArgumentException(`Unknown event: ${name}`);
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * Returns the cartesian product of the given arrays.
     *
     * Example:
     *   cartesian([1, 2], ['a', 'b']); => [[1, 'a'], [1, 'b'], [2, 'a'], [2, 'b']]
     */
    function cartesianProduct(...a) {
        return a.reduce((a, b) => a.flatMap((d) => b.map((e) => [d, e].flat())));
    }
    /** Expands "AllEvents" events into atomic events. */
    function unrollEvents(events) {
        const allEvents = new Set();
        function addEvents(events) {
            for (const event of events) {
                allEvents.add(event);
            }
        }
        for (const event of events) {
            switch (event) {
                case BiDiModule.Bluetooth:
                    addEvents(Object.values(Bluetooth$2.EventNames));
                    break;
                case BiDiModule.BrowsingContext:
                    addEvents(Object.values(BrowsingContext$2.EventNames));
                    break;
                case BiDiModule.Log:
                    addEvents(Object.values(Log$1.EventNames));
                    break;
                case BiDiModule.Network:
                    addEvents(Object.values(Network$2.EventNames));
                    break;
                case BiDiModule.Script:
                    addEvents(Object.values(Script$2.EventNames));
                    break;
                default:
                    allEvents.add(event);
            }
        }
        return [...allEvents.values()];
    }
    class SubscriptionManager {
        #subscriptionPriority = 0;
        // BrowsingContext `null` means the event has subscription across all the
        // browsing contexts.
        // Channel `null` means no `channel` should be added.
        #channelToContextToEventMap = new Map();
        #browsingContextStorage;
        constructor(browsingContextStorage) {
            this.#browsingContextStorage = browsingContextStorage;
        }
        getChannelsSubscribedToEvent(eventMethod, contextId) {
            const prioritiesAndChannels = Array.from(this.#channelToContextToEventMap.keys())
                .map((channel) => ({
                priority: this.#getEventSubscriptionPriorityForChannel(eventMethod, contextId, channel),
                channel,
            }))
                .filter(({ priority }) => priority !== null);
            // Sort channels by priority.
            return prioritiesAndChannels
                .sort((a, b) => a.priority - b.priority)
                .map(({ channel }) => channel);
        }
        #getEventSubscriptionPriorityForChannel(eventMethod, contextId, channel) {
            const contextToEventMap = this.#channelToContextToEventMap.get(channel);
            if (contextToEventMap === undefined) {
                return null;
            }
            const maybeTopLevelContextId = this.#browsingContextStorage.findTopLevelContextId(contextId);
            // `null` covers global subscription.
            const relevantContexts = [...new Set([null, maybeTopLevelContextId])];
            // Get all the subscription priorities.
            const priorities = relevantContexts
                .map((context) => {
                // Get the priority for exact event name
                const priority = contextToEventMap.get(context)?.get(eventMethod);
                // For CDP we can't provide specific event name when subscribing
                // to the module directly.
                // Because of that we need to see event `cdp` exits in the map.
                if (isCdpEvent(eventMethod)) {
                    const cdpPriority = contextToEventMap
                        .get(context)
                        ?.get(BiDiModule.Cdp);
                    // If we subscribe to the event directly and `cdp` module as well
                    // priority will be different we take minimal priority
                    return priority && cdpPriority
                        ? Math.min(priority, cdpPriority)
                        : // At this point we know that we have subscribed
                            // to only one of the two
                            (priority ?? cdpPriority);
                }
                return priority;
            })
                .filter((p) => p !== undefined);
            if (priorities.length === 0) {
                // Not subscribed, return null.
                return null;
            }
            // Return minimal priority.
            return Math.min(...priorities);
        }
        /**
         * @param module BiDi+ module
         * @param contextId `null` == globally subscribed
         *
         * @returns
         */
        isSubscribedTo(moduleOrEvent, contextId = null) {
            const topLevelContext = this.#browsingContextStorage.findTopLevelContextId(contextId);
            for (const browserContextToEventMap of this.#channelToContextToEventMap.values()) {
                for (const [id, eventMap] of browserContextToEventMap.entries()) {
                    // Not subscribed to this context or globally
                    if (topLevelContext !== id && id !== null) {
                        continue;
                    }
                    for (const event of eventMap.keys()) {
                        // This also covers the `cdp` case where
                        // we don't unroll the event names
                        if (
                        // Event explicitly subscribed
                        event === moduleOrEvent ||
                            // Event subscribed via module
                            event === moduleOrEvent.split('.').at(0) ||
                            // Event explicitly subscribed compared to module
                            event.split('.').at(0) === moduleOrEvent) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }
        /**
         * Subscribes to event in the given context and channel.
         * @param {EventNames} event
         * @param {BrowsingContext.BrowsingContext | null} contextId
         * @param {BidiPlusChannel} channel
         * @return {SubscriptionItem[]} List of
         * subscriptions. If the event is a whole module, it will return all the specific
         * events. If the contextId is null, it will return all the top-level contexts which were
         * not subscribed before the command.
         */
        subscribe(event, contextId, channel) {
            // All the subscriptions are handled on the top-level contexts.
            contextId = this.#browsingContextStorage.findTopLevelContextId(contextId);
            // Check if subscribed event is a whole module
            switch (event) {
                case BiDiModule.BrowsingContext:
                    return Object.values(BrowsingContext$2.EventNames)
                        .map((specificEvent) => this.subscribe(specificEvent, contextId, channel))
                        .flat();
                case BiDiModule.Log:
                    return Object.values(Log$1.EventNames)
                        .map((specificEvent) => this.subscribe(specificEvent, contextId, channel))
                        .flat();
                case BiDiModule.Network:
                    return Object.values(Network$2.EventNames)
                        .map((specificEvent) => this.subscribe(specificEvent, contextId, channel))
                        .flat();
                case BiDiModule.Script:
                    return Object.values(Script$2.EventNames)
                        .map((specificEvent) => this.subscribe(specificEvent, contextId, channel))
                        .flat();
                case BiDiModule.Bluetooth:
                    return Object.values(Bluetooth$2.EventNames)
                        .map((specificEvent) => this.subscribe(specificEvent, contextId, channel))
                        .flat();
                // Intentionally left empty.
            }
            if (!this.#channelToContextToEventMap.has(channel)) {
                this.#channelToContextToEventMap.set(channel, new Map());
            }
            const contextToEventMap = this.#channelToContextToEventMap.get(channel);
            if (!contextToEventMap.has(contextId)) {
                contextToEventMap.set(contextId, new Map());
            }
            const eventMap = contextToEventMap.get(contextId);
            const affectedContextIds = (contextId === null
                ? this.#browsingContextStorage.getTopLevelContexts().map((c) => c.id)
                : [contextId])
                // There can be contexts that are already subscribed to the event. Do not include
                // them to the output.
                .filter((contextId) => !this.isSubscribedTo(event, contextId));
            if (!eventMap.has(event)) {
                // Add subscription only if it's not already subscribed.
                eventMap.set(event, this.#subscriptionPriority++);
            }
            return affectedContextIds.map((contextId) => ({
                event,
                contextId,
            }));
        }
        /**
         * Unsubscribes atomically from all events in the given contexts and channel.
         */
        unsubscribeAll(events, contextIds, channel) {
            // Assert all contexts are known.
            for (const contextId of contextIds) {
                if (contextId !== null) {
                    this.#browsingContextStorage.getContext(contextId);
                }
            }
            const eventContextPairs = cartesianProduct(unrollEvents(events), contextIds);
            // Assert all unsubscriptions are valid.
            // If any of the unsubscriptions are invalid, do not unsubscribe from anything.
            eventContextPairs
                .map(([event, contextId]) => this.#checkUnsubscribe(event, contextId, channel))
                .forEach((unsubscribe) => unsubscribe());
        }
        /**
         * Unsubscribes from the event in the given context and channel.
         * Syntactic sugar for "unsubscribeAll".
         */
        unsubscribe(eventName, contextId, channel) {
            this.unsubscribeAll([eventName], [contextId], channel);
        }
        #checkUnsubscribe(event, contextId, channel) {
            // All the subscriptions are handled on the top-level contexts.
            contextId = this.#browsingContextStorage.findTopLevelContextId(contextId);
            if (!this.#channelToContextToEventMap.has(channel)) {
                throw new InvalidArgumentException(`Cannot unsubscribe from ${event}, ${contextId === null ? 'null' : contextId}. No subscription found.`);
            }
            const contextToEventMap = this.#channelToContextToEventMap.get(channel);
            if (!contextToEventMap.has(contextId)) {
                throw new InvalidArgumentException(`Cannot unsubscribe from ${event}, ${contextId === null ? 'null' : contextId}. No subscription found.`);
            }
            const eventMap = contextToEventMap.get(contextId);
            if (!eventMap.has(event)) {
                throw new InvalidArgumentException(`Cannot unsubscribe from ${event}, ${contextId === null ? 'null' : contextId}. No subscription found.`);
            }
            return () => {
                eventMap.delete(event);
                // Clean up maps if empty.
                if (eventMap.size === 0) {
                    contextToEventMap.delete(event);
                }
                if (contextToEventMap.size === 0) {
                    this.#channelToContextToEventMap.delete(channel);
                }
            };
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    var _a$2;
    class EventWrapper {
        #idWrapper = new IdWrapper();
        #contextId;
        #event;
        constructor(event, contextId) {
            this.#event = event;
            this.#contextId = contextId;
        }
        get id() {
            return this.#idWrapper.id;
        }
        get contextId() {
            return this.#contextId;
        }
        get event() {
            return this.#event;
        }
    }
    /**
     * Maps event name to a desired buffer length.
     */
    const eventBufferLength = new Map([[Log$1.EventNames.LogEntryAdded, 100]]);
    class EventManager extends EventEmitter {
        /**
         * Maps event name to a set of contexts where this event already happened.
         * Needed for getting buffered events from all the contexts in case of
         * subscripting to all contexts.
         */
        #eventToContextsMap = new DefaultMap(() => new Set());
        /**
         * Maps `eventName` + `browsingContext` to buffer. Used to get buffered events
         * during subscription. Channel-agnostic.
         */
        #eventBuffers = new Map();
        /**
         * Maps `eventName` + `browsingContext` to  Map of channel to last id
         * Used to avoid sending duplicated events when user
         * subscribes -> unsubscribes -> subscribes.
         */
        #lastMessageSent = new Map();
        #subscriptionManager;
        #browsingContextStorage;
        /**
         * Map of event name to hooks to be called when client is subscribed to the event.
         */
        #subscribeHooks;
        constructor(browsingContextStorage) {
            super();
            this.#browsingContextStorage = browsingContextStorage;
            this.#subscriptionManager = new SubscriptionManager(browsingContextStorage);
            this.#subscribeHooks = new DefaultMap(() => []);
        }
        get subscriptionManager() {
            return this.#subscriptionManager;
        }
        /**
         * Returns consistent key to be used to access value maps.
         */
        static #getMapKey(eventName, browsingContext) {
            return JSON.stringify({ eventName, browsingContext });
        }
        addSubscribeHook(event, hook) {
            this.#subscribeHooks.get(event).push(hook);
        }
        registerEvent(event, contextId) {
            this.registerPromiseEvent(Promise.resolve({
                kind: 'success',
                value: event,
            }), contextId, event.method);
        }
        registerPromiseEvent(event, contextId, eventName) {
            const eventWrapper = new EventWrapper(event, contextId);
            const sortedChannels = this.#subscriptionManager.getChannelsSubscribedToEvent(eventName, contextId);
            this.#bufferEvent(eventWrapper, eventName);
            // Send events to channels in the subscription priority.
            for (const channel of sortedChannels) {
                this.emit("event" /* EventManagerEvents.Event */, {
                    message: OutgoingMessage.createFromPromise(event, channel),
                    event: eventName,
                });
                this.#markEventSent(eventWrapper, channel, eventName);
            }
        }
        async subscribe(eventNames, contextIds, channel) {
            for (const name of eventNames) {
                assertSupportedEvent(name);
            }
            // First check if all the contexts are known.
            for (const contextId of contextIds) {
                if (contextId !== null) {
                    // Assert the context is known. Throw exception otherwise.
                    this.#browsingContextStorage.getContext(contextId);
                }
            }
            // List of the subscription items that were actually added. Each contains a specific
            // event and context. No domain event (like "network") or global context subscription
            // (like null) are included.
            const addedSubscriptionItems = [];
            for (const eventName of eventNames) {
                for (const contextId of contextIds) {
                    addedSubscriptionItems.push(...this.#subscriptionManager.subscribe(eventName, contextId, channel));
                    for (const eventWrapper of this.#getBufferedEvents(eventName, contextId, channel)) {
                        // The order of the events is important.
                        this.emit("event" /* EventManagerEvents.Event */, {
                            message: OutgoingMessage.createFromPromise(eventWrapper.event, channel),
                            event: eventName,
                        });
                        this.#markEventSent(eventWrapper, channel, eventName);
                    }
                }
            }
            // Iterate over all new subscription items and call hooks if any. There can be
            // duplicates, e.g. when subscribing to the whole domain and some specific event in
            // the same time ("network", "network.responseCompleted"). `distinctValues` guarantees
            // that hooks are called only once per pair event + context.
            distinctValues(addedSubscriptionItems).forEach(({ contextId, event }) => {
                this.#subscribeHooks.get(event).forEach((hook) => hook(contextId));
            });
            await this.toggleModulesIfNeeded();
        }
        async unsubscribe(eventNames, contextIds, channel) {
            for (const name of eventNames) {
                assertSupportedEvent(name);
            }
            this.#subscriptionManager.unsubscribeAll(eventNames, contextIds, channel);
            await this.toggleModulesIfNeeded();
        }
        async toggleModulesIfNeeded() {
            // TODO(1): Only update changed subscribers
            // TODO(2): Enable for Worker Targets
            await Promise.all(this.#browsingContextStorage.getAllContexts().map(async (context) => {
                return await context.toggleModulesIfNeeded();
            }));
        }
        clearBufferedEvents(contextId) {
            for (const eventName of eventBufferLength.keys()) {
                const bufferMapKey = _a$2.#getMapKey(eventName, contextId);
                this.#eventBuffers.delete(bufferMapKey);
            }
        }
        /**
         * If the event is buffer-able, put it in the buffer.
         */
        #bufferEvent(eventWrapper, eventName) {
            if (!eventBufferLength.has(eventName)) {
                // Do nothing if the event is no buffer-able.
                return;
            }
            const bufferMapKey = _a$2.#getMapKey(eventName, eventWrapper.contextId);
            if (!this.#eventBuffers.has(bufferMapKey)) {
                this.#eventBuffers.set(bufferMapKey, new Buffer$1(eventBufferLength.get(eventName)));
            }
            this.#eventBuffers.get(bufferMapKey).add(eventWrapper);
            // Add the context to the list of contexts having `eventName` events.
            this.#eventToContextsMap.get(eventName).add(eventWrapper.contextId);
        }
        /**
         * If the event is buffer-able, mark it as sent to the given contextId and channel.
         */
        #markEventSent(eventWrapper, channel, eventName) {
            if (!eventBufferLength.has(eventName)) {
                // Do nothing if the event is no buffer-able.
                return;
            }
            const lastSentMapKey = _a$2.#getMapKey(eventName, eventWrapper.contextId);
            const lastId = Math.max(this.#lastMessageSent.get(lastSentMapKey)?.get(channel) ?? 0, eventWrapper.id);
            const channelMap = this.#lastMessageSent.get(lastSentMapKey);
            if (channelMap) {
                channelMap.set(channel, lastId);
            }
            else {
                this.#lastMessageSent.set(lastSentMapKey, new Map([[channel, lastId]]));
            }
        }
        /**
         * Returns events which are buffered and not yet sent to the given channel events.
         */
        #getBufferedEvents(eventName, contextId, channel) {
            const bufferMapKey = _a$2.#getMapKey(eventName, contextId);
            const lastSentMessageId = this.#lastMessageSent.get(bufferMapKey)?.get(channel) ?? -Infinity;
            const result = this.#eventBuffers
                .get(bufferMapKey)
                ?.get()
                .filter((wrapper) => wrapper.id > lastSentMessageId) ?? [];
            if (contextId === null) {
                // For global subscriptions, events buffered in each context should be sent back.
                Array.from(this.#eventToContextsMap.get(eventName).keys())
                    .filter((_contextId) => 
                // Events without context are already in the result.
                _contextId !== null &&
                    // Events from deleted contexts should not be sent.
                    this.#browsingContextStorage.hasContext(_contextId))
                    .map((_contextId) => this.#getBufferedEvents(eventName, _contextId, channel))
                    .forEach((events) => result.push(...events));
            }
            return result.sort((e1, e2) => e1.id - e2.id);
        }
    }
    _a$2 = EventManager;

    /**
     * Copyright 2021 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    class BidiServer extends EventEmitter {
        #messageQueue;
        #transport;
        #commandProcessor;
        #eventManager;
        #browsingContextStorage = new BrowsingContextStorage();
        #realmStorage = new RealmStorage();
        #preloadScriptStorage = new PreloadScriptStorage();
        #bluetoothProcessor;
        #logger;
        #handleIncomingMessage = (message) => {
            void this.#commandProcessor.processCommand(message).catch((error) => {
                this.#logger?.(LogType.debugError, error);
            });
        };
        #processOutgoingMessage = async (messageEntry) => {
            const message = messageEntry.message;
            if (messageEntry.channel !== null) {
                message['channel'] = messageEntry.channel;
            }
            await this.#transport.sendMessage(message);
        };
        constructor(bidiTransport, cdpConnection, browserCdpClient, selfTargetId, defaultUserContextId, parser, logger) {
            super();
            this.#logger = logger;
            this.#messageQueue = new ProcessingQueue(this.#processOutgoingMessage, this.#logger);
            this.#transport = bidiTransport;
            this.#transport.setOnMessage(this.#handleIncomingMessage);
            this.#eventManager = new EventManager(this.#browsingContextStorage);
            const networkStorage = new NetworkStorage(this.#eventManager, this.#browsingContextStorage, browserCdpClient, logger);
            this.#bluetoothProcessor = new BluetoothProcessor(this.#eventManager, this.#browsingContextStorage);
            this.#commandProcessor = new CommandProcessor(cdpConnection, browserCdpClient, this.#eventManager, this.#browsingContextStorage, this.#realmStorage, this.#preloadScriptStorage, networkStorage, this.#bluetoothProcessor, parser, async (options) => {
                // This is required to ignore certificate errors when service worker is fetched.
                await browserCdpClient.sendCommand('Security.setIgnoreCertificateErrors', {
                    ignore: options.acceptInsecureCerts ?? false,
                });
                new CdpTargetManager(cdpConnection, browserCdpClient, selfTargetId, this.#eventManager, this.#browsingContextStorage, this.#realmStorage, networkStorage, this.#bluetoothProcessor, this.#preloadScriptStorage, defaultUserContextId, options?.unhandledPromptBehavior, logger);
                // Needed to get events about new targets.
                await browserCdpClient.sendCommand('Target.setDiscoverTargets', {
                    discover: true,
                });
                // Needed to automatically attach to new targets.
                await browserCdpClient.sendCommand('Target.setAutoAttach', {
                    autoAttach: true,
                    waitForDebuggerOnStart: true,
                    flatten: true,
                });
                await this.#topLevelContextsLoaded();
            }, this.#logger);
            this.#eventManager.on("event" /* EventManagerEvents.Event */, ({ message, event }) => {
                this.emitOutgoingMessage(message, event);
            });
            this.#commandProcessor.on("response" /* CommandProcessorEvents.Response */, ({ message, event }) => {
                this.emitOutgoingMessage(message, event);
            });
        }
        /**
         * Creates and starts BiDi Mapper instance.
         */
        static async createAndStart(bidiTransport, cdpConnection, browserCdpClient, selfTargetId, parser, logger) {
            // The default context is not exposed in Target.getBrowserContexts but can
            // be observed via Target.getTargets. To determine the default browser
            // context, we check which one is mentioned in Target.getTargets and not in
            // Target.getBrowserContexts.
            const [{ browserContextIds }, { targetInfos }] = await Promise.all([
                browserCdpClient.sendCommand('Target.getBrowserContexts'),
                browserCdpClient.sendCommand('Target.getTargets'),
            ]);
            let defaultUserContextId = 'default';
            for (const info of targetInfos) {
                if (info.browserContextId &&
                    !browserContextIds.includes(info.browserContextId)) {
                    defaultUserContextId = info.browserContextId;
                    break;
                }
            }
            const server = new BidiServer(bidiTransport, cdpConnection, browserCdpClient, selfTargetId, defaultUserContextId, parser, logger);
            return server;
        }
        /**
         * Sends BiDi message.
         */
        emitOutgoingMessage(messageEntry, event) {
            this.#messageQueue.add(messageEntry, event);
        }
        close() {
            this.#transport.close();
        }
        async #topLevelContextsLoaded() {
            await Promise.all(this.#browsingContextStorage
                .getTopLevelContexts()
                .map((c) => c.lifecycleLoaded()));
        }
    }

    /**
     * Copyright 2021 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** A error that will be thrown if/when the connection is closed. */
    class CloseError extends Error {
    }
    /** Represents a high-level CDP connection to the browser. */
    class MapperCdpClient extends EventEmitter {
        #cdpConnection;
        #sessionId;
        constructor(cdpConnection, sessionId) {
            super();
            this.#cdpConnection = cdpConnection;
            this.#sessionId = sessionId;
        }
        get sessionId() {
            return this.#sessionId;
        }
        sendCommand(method, ...params) {
            return this.#cdpConnection.sendCommand(method, params[0], this.#sessionId);
        }
        isCloseError(error) {
            return error instanceof CloseError;
        }
    }

    var _a$1;
    /**
     * Represents a high-level CDP connection to the browser backend.
     *
     * Manages all CdpClients (each backed by a Session ID) instance for each active
     * CDP session.
     */
    class MapperCdpConnection {
        static LOGGER_PREFIX_RECV = `${LogType.cdp}:RECV ◂`;
        static LOGGER_PREFIX_SEND = `${LogType.cdp}:SEND ▸`;
        #mainBrowserCdpClient;
        #transport;
        /** Map from session ID to CdpClient.
         * `undefined` points to the main browser session. */
        #sessionCdpClients = new Map();
        #commandCallbacks = new Map();
        #logger;
        #nextId = 0;
        constructor(transport, logger) {
            this.#transport = transport;
            this.#logger = logger;
            this.#transport.setOnMessage(this.#onMessage);
            // Create default Browser CDP Session.
            this.#mainBrowserCdpClient = this.#createCdpClient(undefined);
        }
        /** Closes the connection to the browser. */
        close() {
            this.#transport.close();
            for (const [, { reject, error }] of this.#commandCallbacks) {
                reject(error);
            }
            this.#commandCallbacks.clear();
            this.#sessionCdpClients.clear();
        }
        async createBrowserSession() {
            const { sessionId } = await this.#mainBrowserCdpClient.sendCommand('Target.attachToBrowserTarget');
            return this.#createCdpClient(sessionId);
        }
        /**
         * Gets a CdpClient instance attached to the given session ID,
         * or null if the session is not attached.
         */
        getCdpClient(sessionId) {
            const cdpClient = this.#sessionCdpClients.get(sessionId);
            if (!cdpClient) {
                throw new Error(`Unknown CDP session ID: ${sessionId}`);
            }
            return cdpClient;
        }
        sendCommand(method, params, sessionId) {
            return new Promise((resolve, reject) => {
                const id = this.#nextId++;
                this.#commandCallbacks.set(id, {
                    resolve,
                    reject,
                    error: new CloseError(`${method} ${JSON.stringify(params)} ${sessionId ?? ''} call rejected because the connection has been closed.`),
                });
                const cdpMessage = { id, method, params };
                if (sessionId) {
                    cdpMessage.sessionId = sessionId;
                }
                void this.#transport
                    .sendMessage(JSON.stringify(cdpMessage))
                    ?.catch((error) => {
                    this.#logger?.(LogType.debugError, error);
                    this.#transport.close();
                });
                this.#logger?.(_a$1.LOGGER_PREFIX_SEND, cdpMessage);
            });
        }
        #onMessage = (json) => {
            const message = JSON.parse(json);
            this.#logger?.(_a$1.LOGGER_PREFIX_RECV, message);
            // Update client map if a session is attached
            // Listen for these events on every session.
            if (message.method === 'Target.attachedToTarget') {
                const { sessionId } = message.params;
                this.#createCdpClient(sessionId);
            }
            if (message.id !== undefined) {
                // Handle command response.
                const callbacks = this.#commandCallbacks.get(message.id);
                this.#commandCallbacks.delete(message.id);
                if (callbacks) {
                    if (message.result) {
                        callbacks.resolve(message.result);
                    }
                    else if (message.error) {
                        callbacks.reject(message.error);
                    }
                }
            }
            else if (message.method) {
                const client = this.#sessionCdpClients.get(message.sessionId ?? undefined);
                client?.emit(message.method, message.params || {});
                // Update client map if a session is detached
                // But emit on that session
                if (message.method === 'Target.detachedFromTarget') {
                    const { sessionId } = message.params;
                    const client = this.#sessionCdpClients.get(sessionId);
                    if (client) {
                        this.#sessionCdpClients.delete(sessionId);
                        client.removeAllListeners();
                    }
                }
            }
        };
        /**
         * Creates a new CdpClient instance for the given session ID.
         * @param sessionId either a string, or undefined for the main browser session.
         * The main browser session is used only to create new browser sessions.
         * @private
         */
        #createCdpClient(sessionId) {
            const cdpClient = new MapperCdpClient(this, sessionId);
            this.#sessionCdpClients.set(sessionId, cdpClient);
            return cdpClient;
        }
    }
    _a$1 = MapperCdpConnection;

    var util;
    (function (util) {
        util.assertEqual = (val) => val;
        function assertIs(_arg) { }
        util.assertIs = assertIs;
        function assertNever(_x) {
            throw new Error();
        }
        util.assertNever = assertNever;
        util.arrayToEnum = (items) => {
            const obj = {};
            for (const item of items) {
                obj[item] = item;
            }
            return obj;
        };
        util.getValidEnumValues = (obj) => {
            const validKeys = util.objectKeys(obj).filter((k) => typeof obj[obj[k]] !== "number");
            const filtered = {};
            for (const k of validKeys) {
                filtered[k] = obj[k];
            }
            return util.objectValues(filtered);
        };
        util.objectValues = (obj) => {
            return util.objectKeys(obj).map(function (e) {
                return obj[e];
            });
        };
        util.objectKeys = typeof Object.keys === "function" // eslint-disable-line ban/ban
            ? (obj) => Object.keys(obj) // eslint-disable-line ban/ban
            : (object) => {
                const keys = [];
                for (const key in object) {
                    if (Object.prototype.hasOwnProperty.call(object, key)) {
                        keys.push(key);
                    }
                }
                return keys;
            };
        util.find = (arr, checker) => {
            for (const item of arr) {
                if (checker(item))
                    return item;
            }
            return undefined;
        };
        util.isInteger = typeof Number.isInteger === "function"
            ? (val) => Number.isInteger(val) // eslint-disable-line ban/ban
            : (val) => typeof val === "number" && isFinite(val) && Math.floor(val) === val;
        function joinValues(array, separator = " | ") {
            return array
                .map((val) => (typeof val === "string" ? `'${val}'` : val))
                .join(separator);
        }
        util.joinValues = joinValues;
        util.jsonStringifyReplacer = (_, value) => {
            if (typeof value === "bigint") {
                return value.toString();
            }
            return value;
        };
    })(util || (util = {}));
    var objectUtil;
    (function (objectUtil) {
        objectUtil.mergeShapes = (first, second) => {
            return {
                ...first,
                ...second, // second overwrites first
            };
        };
    })(objectUtil || (objectUtil = {}));
    const ZodParsedType = util.arrayToEnum([
        "string",
        "nan",
        "number",
        "integer",
        "float",
        "boolean",
        "date",
        "bigint",
        "symbol",
        "function",
        "undefined",
        "null",
        "array",
        "object",
        "unknown",
        "promise",
        "void",
        "never",
        "map",
        "set",
    ]);
    const getParsedType = (data) => {
        const t = typeof data;
        switch (t) {
            case "undefined":
                return ZodParsedType.undefined;
            case "string":
                return ZodParsedType.string;
            case "number":
                return isNaN(data) ? ZodParsedType.nan : ZodParsedType.number;
            case "boolean":
                return ZodParsedType.boolean;
            case "function":
                return ZodParsedType.function;
            case "bigint":
                return ZodParsedType.bigint;
            case "symbol":
                return ZodParsedType.symbol;
            case "object":
                if (Array.isArray(data)) {
                    return ZodParsedType.array;
                }
                if (data === null) {
                    return ZodParsedType.null;
                }
                if (data.then &&
                    typeof data.then === "function" &&
                    data.catch &&
                    typeof data.catch === "function") {
                    return ZodParsedType.promise;
                }
                if (typeof Map !== "undefined" && data instanceof Map) {
                    return ZodParsedType.map;
                }
                if (typeof Set !== "undefined" && data instanceof Set) {
                    return ZodParsedType.set;
                }
                if (typeof Date !== "undefined" && data instanceof Date) {
                    return ZodParsedType.date;
                }
                return ZodParsedType.object;
            default:
                return ZodParsedType.unknown;
        }
    };

    const ZodIssueCode = util.arrayToEnum([
        "invalid_type",
        "invalid_literal",
        "custom",
        "invalid_union",
        "invalid_union_discriminator",
        "invalid_enum_value",
        "unrecognized_keys",
        "invalid_arguments",
        "invalid_return_type",
        "invalid_date",
        "invalid_string",
        "too_small",
        "too_big",
        "invalid_intersection_types",
        "not_multiple_of",
        "not_finite",
    ]);
    const quotelessJson = (obj) => {
        const json = JSON.stringify(obj, null, 2);
        return json.replace(/"([^"]+)":/g, "$1:");
    };
    class ZodError extends Error {
        constructor(issues) {
            super();
            this.issues = [];
            this.addIssue = (sub) => {
                this.issues = [...this.issues, sub];
            };
            this.addIssues = (subs = []) => {
                this.issues = [...this.issues, ...subs];
            };
            const actualProto = new.target.prototype;
            if (Object.setPrototypeOf) {
                // eslint-disable-next-line ban/ban
                Object.setPrototypeOf(this, actualProto);
            }
            else {
                this.__proto__ = actualProto;
            }
            this.name = "ZodError";
            this.issues = issues;
        }
        get errors() {
            return this.issues;
        }
        format(_mapper) {
            const mapper = _mapper ||
                function (issue) {
                    return issue.message;
                };
            const fieldErrors = { _errors: [] };
            const processError = (error) => {
                for (const issue of error.issues) {
                    if (issue.code === "invalid_union") {
                        issue.unionErrors.map(processError);
                    }
                    else if (issue.code === "invalid_return_type") {
                        processError(issue.returnTypeError);
                    }
                    else if (issue.code === "invalid_arguments") {
                        processError(issue.argumentsError);
                    }
                    else if (issue.path.length === 0) {
                        fieldErrors._errors.push(mapper(issue));
                    }
                    else {
                        let curr = fieldErrors;
                        let i = 0;
                        while (i < issue.path.length) {
                            const el = issue.path[i];
                            const terminal = i === issue.path.length - 1;
                            if (!terminal) {
                                curr[el] = curr[el] || { _errors: [] };
                                // if (typeof el === "string") {
                                //   curr[el] = curr[el] || { _errors: [] };
                                // } else if (typeof el === "number") {
                                //   const errorArray: any = [];
                                //   errorArray._errors = [];
                                //   curr[el] = curr[el] || errorArray;
                                // }
                            }
                            else {
                                curr[el] = curr[el] || { _errors: [] };
                                curr[el]._errors.push(mapper(issue));
                            }
                            curr = curr[el];
                            i++;
                        }
                    }
                }
            };
            processError(this);
            return fieldErrors;
        }
        static assert(value) {
            if (!(value instanceof ZodError)) {
                throw new Error(`Not a ZodError: ${value}`);
            }
        }
        toString() {
            return this.message;
        }
        get message() {
            return JSON.stringify(this.issues, util.jsonStringifyReplacer, 2);
        }
        get isEmpty() {
            return this.issues.length === 0;
        }
        flatten(mapper = (issue) => issue.message) {
            const fieldErrors = {};
            const formErrors = [];
            for (const sub of this.issues) {
                if (sub.path.length > 0) {
                    fieldErrors[sub.path[0]] = fieldErrors[sub.path[0]] || [];
                    fieldErrors[sub.path[0]].push(mapper(sub));
                }
                else {
                    formErrors.push(mapper(sub));
                }
            }
            return { formErrors, fieldErrors };
        }
        get formErrors() {
            return this.flatten();
        }
    }
    ZodError.create = (issues) => {
        const error = new ZodError(issues);
        return error;
    };

    const errorMap = (issue, _ctx) => {
        let message;
        switch (issue.code) {
            case ZodIssueCode.invalid_type:
                if (issue.received === ZodParsedType.undefined) {
                    message = "Required";
                }
                else {
                    message = `Expected ${issue.expected}, received ${issue.received}`;
                }
                break;
            case ZodIssueCode.invalid_literal:
                message = `Invalid literal value, expected ${JSON.stringify(issue.expected, util.jsonStringifyReplacer)}`;
                break;
            case ZodIssueCode.unrecognized_keys:
                message = `Unrecognized key(s) in object: ${util.joinValues(issue.keys, ", ")}`;
                break;
            case ZodIssueCode.invalid_union:
                message = `Invalid input`;
                break;
            case ZodIssueCode.invalid_union_discriminator:
                message = `Invalid discriminator value. Expected ${util.joinValues(issue.options)}`;
                break;
            case ZodIssueCode.invalid_enum_value:
                message = `Invalid enum value. Expected ${util.joinValues(issue.options)}, received '${issue.received}'`;
                break;
            case ZodIssueCode.invalid_arguments:
                message = `Invalid function arguments`;
                break;
            case ZodIssueCode.invalid_return_type:
                message = `Invalid function return type`;
                break;
            case ZodIssueCode.invalid_date:
                message = `Invalid date`;
                break;
            case ZodIssueCode.invalid_string:
                if (typeof issue.validation === "object") {
                    if ("includes" in issue.validation) {
                        message = `Invalid input: must include "${issue.validation.includes}"`;
                        if (typeof issue.validation.position === "number") {
                            message = `${message} at one or more positions greater than or equal to ${issue.validation.position}`;
                        }
                    }
                    else if ("startsWith" in issue.validation) {
                        message = `Invalid input: must start with "${issue.validation.startsWith}"`;
                    }
                    else if ("endsWith" in issue.validation) {
                        message = `Invalid input: must end with "${issue.validation.endsWith}"`;
                    }
                    else {
                        util.assertNever(issue.validation);
                    }
                }
                else if (issue.validation !== "regex") {
                    message = `Invalid ${issue.validation}`;
                }
                else {
                    message = "Invalid";
                }
                break;
            case ZodIssueCode.too_small:
                if (issue.type === "array")
                    message = `Array must contain ${issue.exact ? "exactly" : issue.inclusive ? `at least` : `more than`} ${issue.minimum} element(s)`;
                else if (issue.type === "string")
                    message = `String must contain ${issue.exact ? "exactly" : issue.inclusive ? `at least` : `over`} ${issue.minimum} character(s)`;
                else if (issue.type === "number")
                    message = `Number must be ${issue.exact
                    ? `exactly equal to `
                    : issue.inclusive
                        ? `greater than or equal to `
                        : `greater than `}${issue.minimum}`;
                else if (issue.type === "date")
                    message = `Date must be ${issue.exact
                    ? `exactly equal to `
                    : issue.inclusive
                        ? `greater than or equal to `
                        : `greater than `}${new Date(Number(issue.minimum))}`;
                else
                    message = "Invalid input";
                break;
            case ZodIssueCode.too_big:
                if (issue.type === "array")
                    message = `Array must contain ${issue.exact ? `exactly` : issue.inclusive ? `at most` : `less than`} ${issue.maximum} element(s)`;
                else if (issue.type === "string")
                    message = `String must contain ${issue.exact ? `exactly` : issue.inclusive ? `at most` : `under`} ${issue.maximum} character(s)`;
                else if (issue.type === "number")
                    message = `Number must be ${issue.exact
                    ? `exactly`
                    : issue.inclusive
                        ? `less than or equal to`
                        : `less than`} ${issue.maximum}`;
                else if (issue.type === "bigint")
                    message = `BigInt must be ${issue.exact
                    ? `exactly`
                    : issue.inclusive
                        ? `less than or equal to`
                        : `less than`} ${issue.maximum}`;
                else if (issue.type === "date")
                    message = `Date must be ${issue.exact
                    ? `exactly`
                    : issue.inclusive
                        ? `smaller than or equal to`
                        : `smaller than`} ${new Date(Number(issue.maximum))}`;
                else
                    message = "Invalid input";
                break;
            case ZodIssueCode.custom:
                message = `Invalid input`;
                break;
            case ZodIssueCode.invalid_intersection_types:
                message = `Intersection results could not be merged`;
                break;
            case ZodIssueCode.not_multiple_of:
                message = `Number must be a multiple of ${issue.multipleOf}`;
                break;
            case ZodIssueCode.not_finite:
                message = "Number must be finite";
                break;
            default:
                message = _ctx.defaultError;
                util.assertNever(issue);
        }
        return { message };
    };

    let overrideErrorMap = errorMap;
    function setErrorMap(map) {
        overrideErrorMap = map;
    }
    function getErrorMap() {
        return overrideErrorMap;
    }

    const makeIssue = (params) => {
        const { data, path, errorMaps, issueData } = params;
        const fullPath = [...path, ...(issueData.path || [])];
        const fullIssue = {
            ...issueData,
            path: fullPath,
        };
        if (issueData.message !== undefined) {
            return {
                ...issueData,
                path: fullPath,
                message: issueData.message,
            };
        }
        let errorMessage = "";
        const maps = errorMaps
            .filter((m) => !!m)
            .slice()
            .reverse();
        for (const map of maps) {
            errorMessage = map(fullIssue, { data, defaultError: errorMessage }).message;
        }
        return {
            ...issueData,
            path: fullPath,
            message: errorMessage,
        };
    };
    const EMPTY_PATH = [];
    function addIssueToContext(ctx, issueData) {
        const overrideMap = getErrorMap();
        const issue = makeIssue({
            issueData: issueData,
            data: ctx.data,
            path: ctx.path,
            errorMaps: [
                ctx.common.contextualErrorMap,
                ctx.schemaErrorMap,
                overrideMap,
                overrideMap === errorMap ? undefined : errorMap, // then global default map
            ].filter((x) => !!x),
        });
        ctx.common.issues.push(issue);
    }
    class ParseStatus {
        constructor() {
            this.value = "valid";
        }
        dirty() {
            if (this.value === "valid")
                this.value = "dirty";
        }
        abort() {
            if (this.value !== "aborted")
                this.value = "aborted";
        }
        static mergeArray(status, results) {
            const arrayValue = [];
            for (const s of results) {
                if (s.status === "aborted")
                    return INVALID;
                if (s.status === "dirty")
                    status.dirty();
                arrayValue.push(s.value);
            }
            return { status: status.value, value: arrayValue };
        }
        static async mergeObjectAsync(status, pairs) {
            const syncPairs = [];
            for (const pair of pairs) {
                const key = await pair.key;
                const value = await pair.value;
                syncPairs.push({
                    key,
                    value,
                });
            }
            return ParseStatus.mergeObjectSync(status, syncPairs);
        }
        static mergeObjectSync(status, pairs) {
            const finalObject = {};
            for (const pair of pairs) {
                const { key, value } = pair;
                if (key.status === "aborted")
                    return INVALID;
                if (value.status === "aborted")
                    return INVALID;
                if (key.status === "dirty")
                    status.dirty();
                if (value.status === "dirty")
                    status.dirty();
                if (key.value !== "__proto__" &&
                    (typeof value.value !== "undefined" || pair.alwaysSet)) {
                    finalObject[key.value] = value.value;
                }
            }
            return { status: status.value, value: finalObject };
        }
    }
    const INVALID = Object.freeze({
        status: "aborted",
    });
    const DIRTY = (value) => ({ status: "dirty", value });
    const OK = (value) => ({ status: "valid", value });
    const isAborted = (x) => x.status === "aborted";
    const isDirty = (x) => x.status === "dirty";
    const isValid = (x) => x.status === "valid";
    const isAsync = (x) => typeof Promise !== "undefined" && x instanceof Promise;

    /******************************************************************************
    Copyright (c) Microsoft Corporation.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
    REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
    INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
    LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
    OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
    PERFORMANCE OF THIS SOFTWARE.
    ***************************************************************************** */

    function __classPrivateFieldGet(receiver, state, kind, f) {
        if (typeof state === "function" ? receiver !== state || !f : !state.has(receiver)) throw new TypeError("Cannot read private member from an object whose class did not declare it");
        return state.get(receiver);
    }

    function __classPrivateFieldSet(receiver, state, value, kind, f) {
        if (typeof state === "function" ? receiver !== state || !f : !state.has(receiver)) throw new TypeError("Cannot write private member to an object whose class did not declare it");
        return (state.set(receiver, value)), value;
    }

    typeof SuppressedError === "function" ? SuppressedError : function (error, suppressed, message) {
        var e = new Error(message);
        return e.name = "SuppressedError", e.error = error, e.suppressed = suppressed, e;
    };

    var errorUtil;
    (function (errorUtil) {
        errorUtil.errToObj = (message) => typeof message === "string" ? { message } : message || {};
        errorUtil.toString = (message) => typeof message === "string" ? message : message === null || message === void 0 ? void 0 : message.message;
    })(errorUtil || (errorUtil = {}));

    var _ZodEnum_cache, _ZodNativeEnum_cache;
    class ParseInputLazyPath {
        constructor(parent, value, path, key) {
            this._cachedPath = [];
            this.parent = parent;
            this.data = value;
            this._path = path;
            this._key = key;
        }
        get path() {
            if (!this._cachedPath.length) {
                if (this._key instanceof Array) {
                    this._cachedPath.push(...this._path, ...this._key);
                }
                else {
                    this._cachedPath.push(...this._path, this._key);
                }
            }
            return this._cachedPath;
        }
    }
    const handleResult = (ctx, result) => {
        if (isValid(result)) {
            return { success: true, data: result.value };
        }
        else {
            if (!ctx.common.issues.length) {
                throw new Error("Validation failed but no issues detected.");
            }
            return {
                success: false,
                get error() {
                    if (this._error)
                        return this._error;
                    const error = new ZodError(ctx.common.issues);
                    this._error = error;
                    return this._error;
                },
            };
        }
    };
    function processCreateParams(params) {
        if (!params)
            return {};
        const { errorMap, invalid_type_error, required_error, description } = params;
        if (errorMap && (invalid_type_error || required_error)) {
            throw new Error(`Can't use "invalid_type_error" or "required_error" in conjunction with custom error map.`);
        }
        if (errorMap)
            return { errorMap: errorMap, description };
        const customMap = (iss, ctx) => {
            var _a, _b;
            const { message } = params;
            if (iss.code === "invalid_enum_value") {
                return { message: message !== null && message !== void 0 ? message : ctx.defaultError };
            }
            if (typeof ctx.data === "undefined") {
                return { message: (_a = message !== null && message !== void 0 ? message : required_error) !== null && _a !== void 0 ? _a : ctx.defaultError };
            }
            if (iss.code !== "invalid_type")
                return { message: ctx.defaultError };
            return { message: (_b = message !== null && message !== void 0 ? message : invalid_type_error) !== null && _b !== void 0 ? _b : ctx.defaultError };
        };
        return { errorMap: customMap, description };
    }
    class ZodType {
        constructor(def) {
            /** Alias of safeParseAsync */
            this.spa = this.safeParseAsync;
            this._def = def;
            this.parse = this.parse.bind(this);
            this.safeParse = this.safeParse.bind(this);
            this.parseAsync = this.parseAsync.bind(this);
            this.safeParseAsync = this.safeParseAsync.bind(this);
            this.spa = this.spa.bind(this);
            this.refine = this.refine.bind(this);
            this.refinement = this.refinement.bind(this);
            this.superRefine = this.superRefine.bind(this);
            this.optional = this.optional.bind(this);
            this.nullable = this.nullable.bind(this);
            this.nullish = this.nullish.bind(this);
            this.array = this.array.bind(this);
            this.promise = this.promise.bind(this);
            this.or = this.or.bind(this);
            this.and = this.and.bind(this);
            this.transform = this.transform.bind(this);
            this.brand = this.brand.bind(this);
            this.default = this.default.bind(this);
            this.catch = this.catch.bind(this);
            this.describe = this.describe.bind(this);
            this.pipe = this.pipe.bind(this);
            this.readonly = this.readonly.bind(this);
            this.isNullable = this.isNullable.bind(this);
            this.isOptional = this.isOptional.bind(this);
        }
        get description() {
            return this._def.description;
        }
        _getType(input) {
            return getParsedType(input.data);
        }
        _getOrReturnCtx(input, ctx) {
            return (ctx || {
                common: input.parent.common,
                data: input.data,
                parsedType: getParsedType(input.data),
                schemaErrorMap: this._def.errorMap,
                path: input.path,
                parent: input.parent,
            });
        }
        _processInputParams(input) {
            return {
                status: new ParseStatus(),
                ctx: {
                    common: input.parent.common,
                    data: input.data,
                    parsedType: getParsedType(input.data),
                    schemaErrorMap: this._def.errorMap,
                    path: input.path,
                    parent: input.parent,
                },
            };
        }
        _parseSync(input) {
            const result = this._parse(input);
            if (isAsync(result)) {
                throw new Error("Synchronous parse encountered promise.");
            }
            return result;
        }
        _parseAsync(input) {
            const result = this._parse(input);
            return Promise.resolve(result);
        }
        parse(data, params) {
            const result = this.safeParse(data, params);
            if (result.success)
                return result.data;
            throw result.error;
        }
        safeParse(data, params) {
            var _a;
            const ctx = {
                common: {
                    issues: [],
                    async: (_a = params === null || params === void 0 ? void 0 : params.async) !== null && _a !== void 0 ? _a : false,
                    contextualErrorMap: params === null || params === void 0 ? void 0 : params.errorMap,
                },
                path: (params === null || params === void 0 ? void 0 : params.path) || [],
                schemaErrorMap: this._def.errorMap,
                parent: null,
                data,
                parsedType: getParsedType(data),
            };
            const result = this._parseSync({ data, path: ctx.path, parent: ctx });
            return handleResult(ctx, result);
        }
        async parseAsync(data, params) {
            const result = await this.safeParseAsync(data, params);
            if (result.success)
                return result.data;
            throw result.error;
        }
        async safeParseAsync(data, params) {
            const ctx = {
                common: {
                    issues: [],
                    contextualErrorMap: params === null || params === void 0 ? void 0 : params.errorMap,
                    async: true,
                },
                path: (params === null || params === void 0 ? void 0 : params.path) || [],
                schemaErrorMap: this._def.errorMap,
                parent: null,
                data,
                parsedType: getParsedType(data),
            };
            const maybeAsyncResult = this._parse({ data, path: ctx.path, parent: ctx });
            const result = await (isAsync(maybeAsyncResult)
                ? maybeAsyncResult
                : Promise.resolve(maybeAsyncResult));
            return handleResult(ctx, result);
        }
        refine(check, message) {
            const getIssueProperties = (val) => {
                if (typeof message === "string" || typeof message === "undefined") {
                    return { message };
                }
                else if (typeof message === "function") {
                    return message(val);
                }
                else {
                    return message;
                }
            };
            return this._refinement((val, ctx) => {
                const result = check(val);
                const setError = () => ctx.addIssue({
                    code: ZodIssueCode.custom,
                    ...getIssueProperties(val),
                });
                if (typeof Promise !== "undefined" && result instanceof Promise) {
                    return result.then((data) => {
                        if (!data) {
                            setError();
                            return false;
                        }
                        else {
                            return true;
                        }
                    });
                }
                if (!result) {
                    setError();
                    return false;
                }
                else {
                    return true;
                }
            });
        }
        refinement(check, refinementData) {
            return this._refinement((val, ctx) => {
                if (!check(val)) {
                    ctx.addIssue(typeof refinementData === "function"
                        ? refinementData(val, ctx)
                        : refinementData);
                    return false;
                }
                else {
                    return true;
                }
            });
        }
        _refinement(refinement) {
            return new ZodEffects({
                schema: this,
                typeName: ZodFirstPartyTypeKind.ZodEffects,
                effect: { type: "refinement", refinement },
            });
        }
        superRefine(refinement) {
            return this._refinement(refinement);
        }
        optional() {
            return ZodOptional.create(this, this._def);
        }
        nullable() {
            return ZodNullable.create(this, this._def);
        }
        nullish() {
            return this.nullable().optional();
        }
        array() {
            return ZodArray.create(this, this._def);
        }
        promise() {
            return ZodPromise.create(this, this._def);
        }
        or(option) {
            return ZodUnion.create([this, option], this._def);
        }
        and(incoming) {
            return ZodIntersection.create(this, incoming, this._def);
        }
        transform(transform) {
            return new ZodEffects({
                ...processCreateParams(this._def),
                schema: this,
                typeName: ZodFirstPartyTypeKind.ZodEffects,
                effect: { type: "transform", transform },
            });
        }
        default(def) {
            const defaultValueFunc = typeof def === "function" ? def : () => def;
            return new ZodDefault({
                ...processCreateParams(this._def),
                innerType: this,
                defaultValue: defaultValueFunc,
                typeName: ZodFirstPartyTypeKind.ZodDefault,
            });
        }
        brand() {
            return new ZodBranded({
                typeName: ZodFirstPartyTypeKind.ZodBranded,
                type: this,
                ...processCreateParams(this._def),
            });
        }
        catch(def) {
            const catchValueFunc = typeof def === "function" ? def : () => def;
            return new ZodCatch({
                ...processCreateParams(this._def),
                innerType: this,
                catchValue: catchValueFunc,
                typeName: ZodFirstPartyTypeKind.ZodCatch,
            });
        }
        describe(description) {
            const This = this.constructor;
            return new This({
                ...this._def,
                description,
            });
        }
        pipe(target) {
            return ZodPipeline.create(this, target);
        }
        readonly() {
            return ZodReadonly.create(this);
        }
        isOptional() {
            return this.safeParse(undefined).success;
        }
        isNullable() {
            return this.safeParse(null).success;
        }
    }
    const cuidRegex = /^c[^\s-]{8,}$/i;
    const cuid2Regex = /^[0-9a-z]+$/;
    const ulidRegex = /^[0-9A-HJKMNP-TV-Z]{26}$/;
    // const uuidRegex =
    //   /^([a-f0-9]{8}-[a-f0-9]{4}-[1-5][a-f0-9]{3}-[a-f0-9]{4}-[a-f0-9]{12}|00000000-0000-0000-0000-000000000000)$/i;
    const uuidRegex = /^[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{12}$/i;
    const nanoidRegex = /^[a-z0-9_-]{21}$/i;
    const durationRegex = /^[-+]?P(?!$)(?:(?:[-+]?\d+Y)|(?:[-+]?\d+[.,]\d+Y$))?(?:(?:[-+]?\d+M)|(?:[-+]?\d+[.,]\d+M$))?(?:(?:[-+]?\d+W)|(?:[-+]?\d+[.,]\d+W$))?(?:(?:[-+]?\d+D)|(?:[-+]?\d+[.,]\d+D$))?(?:T(?=[\d+-])(?:(?:[-+]?\d+H)|(?:[-+]?\d+[.,]\d+H$))?(?:(?:[-+]?\d+M)|(?:[-+]?\d+[.,]\d+M$))?(?:[-+]?\d+(?:[.,]\d+)?S)?)??$/;
    // from https://stackoverflow.com/a/46181/1550155
    // old version: too slow, didn't support unicode
    // const emailRegex = /^((([a-z]|\d|[!#\$%&'\*\+\-\/=\?\^_`{\|}~]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])+(\.([a-z]|\d|[!#\$%&'\*\+\-\/=\?\^_`{\|}~]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])+)*)|((\x22)((((\x20|\x09)*(\x0d\x0a))?(\x20|\x09)+)?(([\x01-\x08\x0b\x0c\x0e-\x1f\x7f]|\x21|[\x23-\x5b]|[\x5d-\x7e]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(\\([\x01-\x09\x0b\x0c\x0d-\x7f]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]))))*(((\x20|\x09)*(\x0d\x0a))?(\x20|\x09)+)?(\x22)))@((([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.)+(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))$/i;
    //old email regex
    // const emailRegex = /^(([^<>()[\].,;:\s@"]+(\.[^<>()[\].,;:\s@"]+)*)|(".+"))@((?!-)([^<>()[\].,;:\s@"]+\.)+[^<>()[\].,;:\s@"]{1,})[^-<>()[\].,;:\s@"]$/i;
    // eslint-disable-next-line
    // const emailRegex =
    //   /^(([^<>()[\]\\.,;:\s@\"]+(\.[^<>()[\]\\.,;:\s@\"]+)*)|(\".+\"))@((\[(((25[0-5])|(2[0-4][0-9])|(1[0-9]{2})|([0-9]{1,2}))\.){3}((25[0-5])|(2[0-4][0-9])|(1[0-9]{2})|([0-9]{1,2}))\])|(\[IPv6:(([a-f0-9]{1,4}:){7}|::([a-f0-9]{1,4}:){0,6}|([a-f0-9]{1,4}:){1}:([a-f0-9]{1,4}:){0,5}|([a-f0-9]{1,4}:){2}:([a-f0-9]{1,4}:){0,4}|([a-f0-9]{1,4}:){3}:([a-f0-9]{1,4}:){0,3}|([a-f0-9]{1,4}:){4}:([a-f0-9]{1,4}:){0,2}|([a-f0-9]{1,4}:){5}:([a-f0-9]{1,4}:){0,1})([a-f0-9]{1,4}|(((25[0-5])|(2[0-4][0-9])|(1[0-9]{2})|([0-9]{1,2}))\.){3}((25[0-5])|(2[0-4][0-9])|(1[0-9]{2})|([0-9]{1,2})))\])|([A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])*(\.[A-Za-z]{2,})+))$/;
    // const emailRegex =
    //   /^[a-zA-Z0-9\.\!\#\$\%\&\'\*\+\/\=\?\^\_\`\{\|\}\~\-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$/;
    // const emailRegex =
    //   /^(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\])$/i;
    const emailRegex = /^(?!\.)(?!.*\.\.)([A-Z0-9_'+\-\.]*)[A-Z0-9_+-]@([A-Z0-9][A-Z0-9\-]*\.)+[A-Z]{2,}$/i;
    // const emailRegex =
    //   /^[a-z0-9.!#$%&’*+/=?^_`{|}~-]+@[a-z0-9-]+(?:\.[a-z0-9\-]+)*$/i;
    // from https://thekevinscott.com/emojis-in-javascript/#writing-a-regular-expression
    const _emojiRegex = `^(\\p{Extended_Pictographic}|\\p{Emoji_Component})+$`;
    let emojiRegex;
    // faster, simpler, safer
    const ipv4Regex = /^(?:(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])$/;
    const ipv6Regex = /^(([a-f0-9]{1,4}:){7}|::([a-f0-9]{1,4}:){0,6}|([a-f0-9]{1,4}:){1}:([a-f0-9]{1,4}:){0,5}|([a-f0-9]{1,4}:){2}:([a-f0-9]{1,4}:){0,4}|([a-f0-9]{1,4}:){3}:([a-f0-9]{1,4}:){0,3}|([a-f0-9]{1,4}:){4}:([a-f0-9]{1,4}:){0,2}|([a-f0-9]{1,4}:){5}:([a-f0-9]{1,4}:){0,1})([a-f0-9]{1,4}|(((25[0-5])|(2[0-4][0-9])|(1[0-9]{2})|([0-9]{1,2}))\.){3}((25[0-5])|(2[0-4][0-9])|(1[0-9]{2})|([0-9]{1,2})))$/;
    // https://stackoverflow.com/questions/7860392/determine-if-string-is-in-base64-using-javascript
    const base64Regex = /^([0-9a-zA-Z+/]{4})*(([0-9a-zA-Z+/]{2}==)|([0-9a-zA-Z+/]{3}=))?$/;
    // simple
    // const dateRegexSource = `\\d{4}-\\d{2}-\\d{2}`;
    // no leap year validation
    // const dateRegexSource = `\\d{4}-((0[13578]|10|12)-31|(0[13-9]|1[0-2])-30|(0[1-9]|1[0-2])-(0[1-9]|1\\d|2\\d))`;
    // with leap year validation
    const dateRegexSource = `((\\d\\d[2468][048]|\\d\\d[13579][26]|\\d\\d0[48]|[02468][048]00|[13579][26]00)-02-29|\\d{4}-((0[13578]|1[02])-(0[1-9]|[12]\\d|3[01])|(0[469]|11)-(0[1-9]|[12]\\d|30)|(02)-(0[1-9]|1\\d|2[0-8])))`;
    const dateRegex = new RegExp(`^${dateRegexSource}$`);
    function timeRegexSource(args) {
        // let regex = `\\d{2}:\\d{2}:\\d{2}`;
        let regex = `([01]\\d|2[0-3]):[0-5]\\d:[0-5]\\d`;
        if (args.precision) {
            regex = `${regex}\\.\\d{${args.precision}}`;
        }
        else if (args.precision == null) {
            regex = `${regex}(\\.\\d+)?`;
        }
        return regex;
    }
    function timeRegex(args) {
        return new RegExp(`^${timeRegexSource(args)}$`);
    }
    // Adapted from https://stackoverflow.com/a/3143231
    function datetimeRegex(args) {
        let regex = `${dateRegexSource}T${timeRegexSource(args)}`;
        const opts = [];
        opts.push(args.local ? `Z?` : `Z`);
        if (args.offset)
            opts.push(`([+-]\\d{2}:?\\d{2})`);
        regex = `${regex}(${opts.join("|")})`;
        return new RegExp(`^${regex}$`);
    }
    function isValidIP(ip, version) {
        if ((version === "v4" || !version) && ipv4Regex.test(ip)) {
            return true;
        }
        if ((version === "v6" || !version) && ipv6Regex.test(ip)) {
            return true;
        }
        return false;
    }
    class ZodString extends ZodType {
        _parse(input) {
            if (this._def.coerce) {
                input.data = String(input.data);
            }
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.string) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.string,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const status = new ParseStatus();
            let ctx = undefined;
            for (const check of this._def.checks) {
                if (check.kind === "min") {
                    if (input.data.length < check.value) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_small,
                            minimum: check.value,
                            type: "string",
                            inclusive: true,
                            exact: false,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "max") {
                    if (input.data.length > check.value) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_big,
                            maximum: check.value,
                            type: "string",
                            inclusive: true,
                            exact: false,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "length") {
                    const tooBig = input.data.length > check.value;
                    const tooSmall = input.data.length < check.value;
                    if (tooBig || tooSmall) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        if (tooBig) {
                            addIssueToContext(ctx, {
                                code: ZodIssueCode.too_big,
                                maximum: check.value,
                                type: "string",
                                inclusive: true,
                                exact: true,
                                message: check.message,
                            });
                        }
                        else if (tooSmall) {
                            addIssueToContext(ctx, {
                                code: ZodIssueCode.too_small,
                                minimum: check.value,
                                type: "string",
                                inclusive: true,
                                exact: true,
                                message: check.message,
                            });
                        }
                        status.dirty();
                    }
                }
                else if (check.kind === "email") {
                    if (!emailRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "email",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "emoji") {
                    if (!emojiRegex) {
                        emojiRegex = new RegExp(_emojiRegex, "u");
                    }
                    if (!emojiRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "emoji",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "uuid") {
                    if (!uuidRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "uuid",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "nanoid") {
                    if (!nanoidRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "nanoid",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "cuid") {
                    if (!cuidRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "cuid",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "cuid2") {
                    if (!cuid2Regex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "cuid2",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "ulid") {
                    if (!ulidRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "ulid",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "url") {
                    try {
                        new URL(input.data);
                    }
                    catch (_a) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "url",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "regex") {
                    check.regex.lastIndex = 0;
                    const testResult = check.regex.test(input.data);
                    if (!testResult) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "regex",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "trim") {
                    input.data = input.data.trim();
                }
                else if (check.kind === "includes") {
                    if (!input.data.includes(check.value, check.position)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_string,
                            validation: { includes: check.value, position: check.position },
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "toLowerCase") {
                    input.data = input.data.toLowerCase();
                }
                else if (check.kind === "toUpperCase") {
                    input.data = input.data.toUpperCase();
                }
                else if (check.kind === "startsWith") {
                    if (!input.data.startsWith(check.value)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_string,
                            validation: { startsWith: check.value },
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "endsWith") {
                    if (!input.data.endsWith(check.value)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_string,
                            validation: { endsWith: check.value },
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "datetime") {
                    const regex = datetimeRegex(check);
                    if (!regex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_string,
                            validation: "datetime",
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "date") {
                    const regex = dateRegex;
                    if (!regex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_string,
                            validation: "date",
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "time") {
                    const regex = timeRegex(check);
                    if (!regex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_string,
                            validation: "time",
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "duration") {
                    if (!durationRegex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "duration",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "ip") {
                    if (!isValidIP(input.data, check.version)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "ip",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "base64") {
                    if (!base64Regex.test(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            validation: "base64",
                            code: ZodIssueCode.invalid_string,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else {
                    util.assertNever(check);
                }
            }
            return { status: status.value, value: input.data };
        }
        _regex(regex, validation, message) {
            return this.refinement((data) => regex.test(data), {
                validation,
                code: ZodIssueCode.invalid_string,
                ...errorUtil.errToObj(message),
            });
        }
        _addCheck(check) {
            return new ZodString({
                ...this._def,
                checks: [...this._def.checks, check],
            });
        }
        email(message) {
            return this._addCheck({ kind: "email", ...errorUtil.errToObj(message) });
        }
        url(message) {
            return this._addCheck({ kind: "url", ...errorUtil.errToObj(message) });
        }
        emoji(message) {
            return this._addCheck({ kind: "emoji", ...errorUtil.errToObj(message) });
        }
        uuid(message) {
            return this._addCheck({ kind: "uuid", ...errorUtil.errToObj(message) });
        }
        nanoid(message) {
            return this._addCheck({ kind: "nanoid", ...errorUtil.errToObj(message) });
        }
        cuid(message) {
            return this._addCheck({ kind: "cuid", ...errorUtil.errToObj(message) });
        }
        cuid2(message) {
            return this._addCheck({ kind: "cuid2", ...errorUtil.errToObj(message) });
        }
        ulid(message) {
            return this._addCheck({ kind: "ulid", ...errorUtil.errToObj(message) });
        }
        base64(message) {
            return this._addCheck({ kind: "base64", ...errorUtil.errToObj(message) });
        }
        ip(options) {
            return this._addCheck({ kind: "ip", ...errorUtil.errToObj(options) });
        }
        datetime(options) {
            var _a, _b;
            if (typeof options === "string") {
                return this._addCheck({
                    kind: "datetime",
                    precision: null,
                    offset: false,
                    local: false,
                    message: options,
                });
            }
            return this._addCheck({
                kind: "datetime",
                precision: typeof (options === null || options === void 0 ? void 0 : options.precision) === "undefined" ? null : options === null || options === void 0 ? void 0 : options.precision,
                offset: (_a = options === null || options === void 0 ? void 0 : options.offset) !== null && _a !== void 0 ? _a : false,
                local: (_b = options === null || options === void 0 ? void 0 : options.local) !== null && _b !== void 0 ? _b : false,
                ...errorUtil.errToObj(options === null || options === void 0 ? void 0 : options.message),
            });
        }
        date(message) {
            return this._addCheck({ kind: "date", message });
        }
        time(options) {
            if (typeof options === "string") {
                return this._addCheck({
                    kind: "time",
                    precision: null,
                    message: options,
                });
            }
            return this._addCheck({
                kind: "time",
                precision: typeof (options === null || options === void 0 ? void 0 : options.precision) === "undefined" ? null : options === null || options === void 0 ? void 0 : options.precision,
                ...errorUtil.errToObj(options === null || options === void 0 ? void 0 : options.message),
            });
        }
        duration(message) {
            return this._addCheck({ kind: "duration", ...errorUtil.errToObj(message) });
        }
        regex(regex, message) {
            return this._addCheck({
                kind: "regex",
                regex: regex,
                ...errorUtil.errToObj(message),
            });
        }
        includes(value, options) {
            return this._addCheck({
                kind: "includes",
                value: value,
                position: options === null || options === void 0 ? void 0 : options.position,
                ...errorUtil.errToObj(options === null || options === void 0 ? void 0 : options.message),
            });
        }
        startsWith(value, message) {
            return this._addCheck({
                kind: "startsWith",
                value: value,
                ...errorUtil.errToObj(message),
            });
        }
        endsWith(value, message) {
            return this._addCheck({
                kind: "endsWith",
                value: value,
                ...errorUtil.errToObj(message),
            });
        }
        min(minLength, message) {
            return this._addCheck({
                kind: "min",
                value: minLength,
                ...errorUtil.errToObj(message),
            });
        }
        max(maxLength, message) {
            return this._addCheck({
                kind: "max",
                value: maxLength,
                ...errorUtil.errToObj(message),
            });
        }
        length(len, message) {
            return this._addCheck({
                kind: "length",
                value: len,
                ...errorUtil.errToObj(message),
            });
        }
        /**
         * @deprecated Use z.string().min(1) instead.
         * @see {@link ZodString.min}
         */
        nonempty(message) {
            return this.min(1, errorUtil.errToObj(message));
        }
        trim() {
            return new ZodString({
                ...this._def,
                checks: [...this._def.checks, { kind: "trim" }],
            });
        }
        toLowerCase() {
            return new ZodString({
                ...this._def,
                checks: [...this._def.checks, { kind: "toLowerCase" }],
            });
        }
        toUpperCase() {
            return new ZodString({
                ...this._def,
                checks: [...this._def.checks, { kind: "toUpperCase" }],
            });
        }
        get isDatetime() {
            return !!this._def.checks.find((ch) => ch.kind === "datetime");
        }
        get isDate() {
            return !!this._def.checks.find((ch) => ch.kind === "date");
        }
        get isTime() {
            return !!this._def.checks.find((ch) => ch.kind === "time");
        }
        get isDuration() {
            return !!this._def.checks.find((ch) => ch.kind === "duration");
        }
        get isEmail() {
            return !!this._def.checks.find((ch) => ch.kind === "email");
        }
        get isURL() {
            return !!this._def.checks.find((ch) => ch.kind === "url");
        }
        get isEmoji() {
            return !!this._def.checks.find((ch) => ch.kind === "emoji");
        }
        get isUUID() {
            return !!this._def.checks.find((ch) => ch.kind === "uuid");
        }
        get isNANOID() {
            return !!this._def.checks.find((ch) => ch.kind === "nanoid");
        }
        get isCUID() {
            return !!this._def.checks.find((ch) => ch.kind === "cuid");
        }
        get isCUID2() {
            return !!this._def.checks.find((ch) => ch.kind === "cuid2");
        }
        get isULID() {
            return !!this._def.checks.find((ch) => ch.kind === "ulid");
        }
        get isIP() {
            return !!this._def.checks.find((ch) => ch.kind === "ip");
        }
        get isBase64() {
            return !!this._def.checks.find((ch) => ch.kind === "base64");
        }
        get minLength() {
            let min = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "min") {
                    if (min === null || ch.value > min)
                        min = ch.value;
                }
            }
            return min;
        }
        get maxLength() {
            let max = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "max") {
                    if (max === null || ch.value < max)
                        max = ch.value;
                }
            }
            return max;
        }
    }
    ZodString.create = (params) => {
        var _a;
        return new ZodString({
            checks: [],
            typeName: ZodFirstPartyTypeKind.ZodString,
            coerce: (_a = params === null || params === void 0 ? void 0 : params.coerce) !== null && _a !== void 0 ? _a : false,
            ...processCreateParams(params),
        });
    };
    // https://stackoverflow.com/questions/3966484/why-does-modulus-operator-return-fractional-number-in-javascript/31711034#31711034
    function floatSafeRemainder(val, step) {
        const valDecCount = (val.toString().split(".")[1] || "").length;
        const stepDecCount = (step.toString().split(".")[1] || "").length;
        const decCount = valDecCount > stepDecCount ? valDecCount : stepDecCount;
        const valInt = parseInt(val.toFixed(decCount).replace(".", ""));
        const stepInt = parseInt(step.toFixed(decCount).replace(".", ""));
        return (valInt % stepInt) / Math.pow(10, decCount);
    }
    class ZodNumber extends ZodType {
        constructor() {
            super(...arguments);
            this.min = this.gte;
            this.max = this.lte;
            this.step = this.multipleOf;
        }
        _parse(input) {
            if (this._def.coerce) {
                input.data = Number(input.data);
            }
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.number) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.number,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            let ctx = undefined;
            const status = new ParseStatus();
            for (const check of this._def.checks) {
                if (check.kind === "int") {
                    if (!util.isInteger(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.invalid_type,
                            expected: "integer",
                            received: "float",
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "min") {
                    const tooSmall = check.inclusive
                        ? input.data < check.value
                        : input.data <= check.value;
                    if (tooSmall) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_small,
                            minimum: check.value,
                            type: "number",
                            inclusive: check.inclusive,
                            exact: false,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "max") {
                    const tooBig = check.inclusive
                        ? input.data > check.value
                        : input.data >= check.value;
                    if (tooBig) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_big,
                            maximum: check.value,
                            type: "number",
                            inclusive: check.inclusive,
                            exact: false,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "multipleOf") {
                    if (floatSafeRemainder(input.data, check.value) !== 0) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.not_multiple_of,
                            multipleOf: check.value,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "finite") {
                    if (!Number.isFinite(input.data)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.not_finite,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else {
                    util.assertNever(check);
                }
            }
            return { status: status.value, value: input.data };
        }
        gte(value, message) {
            return this.setLimit("min", value, true, errorUtil.toString(message));
        }
        gt(value, message) {
            return this.setLimit("min", value, false, errorUtil.toString(message));
        }
        lte(value, message) {
            return this.setLimit("max", value, true, errorUtil.toString(message));
        }
        lt(value, message) {
            return this.setLimit("max", value, false, errorUtil.toString(message));
        }
        setLimit(kind, value, inclusive, message) {
            return new ZodNumber({
                ...this._def,
                checks: [
                    ...this._def.checks,
                    {
                        kind,
                        value,
                        inclusive,
                        message: errorUtil.toString(message),
                    },
                ],
            });
        }
        _addCheck(check) {
            return new ZodNumber({
                ...this._def,
                checks: [...this._def.checks, check],
            });
        }
        int(message) {
            return this._addCheck({
                kind: "int",
                message: errorUtil.toString(message),
            });
        }
        positive(message) {
            return this._addCheck({
                kind: "min",
                value: 0,
                inclusive: false,
                message: errorUtil.toString(message),
            });
        }
        negative(message) {
            return this._addCheck({
                kind: "max",
                value: 0,
                inclusive: false,
                message: errorUtil.toString(message),
            });
        }
        nonpositive(message) {
            return this._addCheck({
                kind: "max",
                value: 0,
                inclusive: true,
                message: errorUtil.toString(message),
            });
        }
        nonnegative(message) {
            return this._addCheck({
                kind: "min",
                value: 0,
                inclusive: true,
                message: errorUtil.toString(message),
            });
        }
        multipleOf(value, message) {
            return this._addCheck({
                kind: "multipleOf",
                value: value,
                message: errorUtil.toString(message),
            });
        }
        finite(message) {
            return this._addCheck({
                kind: "finite",
                message: errorUtil.toString(message),
            });
        }
        safe(message) {
            return this._addCheck({
                kind: "min",
                inclusive: true,
                value: Number.MIN_SAFE_INTEGER,
                message: errorUtil.toString(message),
            })._addCheck({
                kind: "max",
                inclusive: true,
                value: Number.MAX_SAFE_INTEGER,
                message: errorUtil.toString(message),
            });
        }
        get minValue() {
            let min = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "min") {
                    if (min === null || ch.value > min)
                        min = ch.value;
                }
            }
            return min;
        }
        get maxValue() {
            let max = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "max") {
                    if (max === null || ch.value < max)
                        max = ch.value;
                }
            }
            return max;
        }
        get isInt() {
            return !!this._def.checks.find((ch) => ch.kind === "int" ||
                (ch.kind === "multipleOf" && util.isInteger(ch.value)));
        }
        get isFinite() {
            let max = null, min = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "finite" ||
                    ch.kind === "int" ||
                    ch.kind === "multipleOf") {
                    return true;
                }
                else if (ch.kind === "min") {
                    if (min === null || ch.value > min)
                        min = ch.value;
                }
                else if (ch.kind === "max") {
                    if (max === null || ch.value < max)
                        max = ch.value;
                }
            }
            return Number.isFinite(min) && Number.isFinite(max);
        }
    }
    ZodNumber.create = (params) => {
        return new ZodNumber({
            checks: [],
            typeName: ZodFirstPartyTypeKind.ZodNumber,
            coerce: (params === null || params === void 0 ? void 0 : params.coerce) || false,
            ...processCreateParams(params),
        });
    };
    class ZodBigInt extends ZodType {
        constructor() {
            super(...arguments);
            this.min = this.gte;
            this.max = this.lte;
        }
        _parse(input) {
            if (this._def.coerce) {
                input.data = BigInt(input.data);
            }
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.bigint) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.bigint,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            let ctx = undefined;
            const status = new ParseStatus();
            for (const check of this._def.checks) {
                if (check.kind === "min") {
                    const tooSmall = check.inclusive
                        ? input.data < check.value
                        : input.data <= check.value;
                    if (tooSmall) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_small,
                            type: "bigint",
                            minimum: check.value,
                            inclusive: check.inclusive,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "max") {
                    const tooBig = check.inclusive
                        ? input.data > check.value
                        : input.data >= check.value;
                    if (tooBig) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_big,
                            type: "bigint",
                            maximum: check.value,
                            inclusive: check.inclusive,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "multipleOf") {
                    if (input.data % check.value !== BigInt(0)) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.not_multiple_of,
                            multipleOf: check.value,
                            message: check.message,
                        });
                        status.dirty();
                    }
                }
                else {
                    util.assertNever(check);
                }
            }
            return { status: status.value, value: input.data };
        }
        gte(value, message) {
            return this.setLimit("min", value, true, errorUtil.toString(message));
        }
        gt(value, message) {
            return this.setLimit("min", value, false, errorUtil.toString(message));
        }
        lte(value, message) {
            return this.setLimit("max", value, true, errorUtil.toString(message));
        }
        lt(value, message) {
            return this.setLimit("max", value, false, errorUtil.toString(message));
        }
        setLimit(kind, value, inclusive, message) {
            return new ZodBigInt({
                ...this._def,
                checks: [
                    ...this._def.checks,
                    {
                        kind,
                        value,
                        inclusive,
                        message: errorUtil.toString(message),
                    },
                ],
            });
        }
        _addCheck(check) {
            return new ZodBigInt({
                ...this._def,
                checks: [...this._def.checks, check],
            });
        }
        positive(message) {
            return this._addCheck({
                kind: "min",
                value: BigInt(0),
                inclusive: false,
                message: errorUtil.toString(message),
            });
        }
        negative(message) {
            return this._addCheck({
                kind: "max",
                value: BigInt(0),
                inclusive: false,
                message: errorUtil.toString(message),
            });
        }
        nonpositive(message) {
            return this._addCheck({
                kind: "max",
                value: BigInt(0),
                inclusive: true,
                message: errorUtil.toString(message),
            });
        }
        nonnegative(message) {
            return this._addCheck({
                kind: "min",
                value: BigInt(0),
                inclusive: true,
                message: errorUtil.toString(message),
            });
        }
        multipleOf(value, message) {
            return this._addCheck({
                kind: "multipleOf",
                value,
                message: errorUtil.toString(message),
            });
        }
        get minValue() {
            let min = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "min") {
                    if (min === null || ch.value > min)
                        min = ch.value;
                }
            }
            return min;
        }
        get maxValue() {
            let max = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "max") {
                    if (max === null || ch.value < max)
                        max = ch.value;
                }
            }
            return max;
        }
    }
    ZodBigInt.create = (params) => {
        var _a;
        return new ZodBigInt({
            checks: [],
            typeName: ZodFirstPartyTypeKind.ZodBigInt,
            coerce: (_a = params === null || params === void 0 ? void 0 : params.coerce) !== null && _a !== void 0 ? _a : false,
            ...processCreateParams(params),
        });
    };
    class ZodBoolean extends ZodType {
        _parse(input) {
            if (this._def.coerce) {
                input.data = Boolean(input.data);
            }
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.boolean) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.boolean,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            return OK(input.data);
        }
    }
    ZodBoolean.create = (params) => {
        return new ZodBoolean({
            typeName: ZodFirstPartyTypeKind.ZodBoolean,
            coerce: (params === null || params === void 0 ? void 0 : params.coerce) || false,
            ...processCreateParams(params),
        });
    };
    class ZodDate extends ZodType {
        _parse(input) {
            if (this._def.coerce) {
                input.data = new Date(input.data);
            }
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.date) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.date,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            if (isNaN(input.data.getTime())) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_date,
                });
                return INVALID;
            }
            const status = new ParseStatus();
            let ctx = undefined;
            for (const check of this._def.checks) {
                if (check.kind === "min") {
                    if (input.data.getTime() < check.value) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_small,
                            message: check.message,
                            inclusive: true,
                            exact: false,
                            minimum: check.value,
                            type: "date",
                        });
                        status.dirty();
                    }
                }
                else if (check.kind === "max") {
                    if (input.data.getTime() > check.value) {
                        ctx = this._getOrReturnCtx(input, ctx);
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.too_big,
                            message: check.message,
                            inclusive: true,
                            exact: false,
                            maximum: check.value,
                            type: "date",
                        });
                        status.dirty();
                    }
                }
                else {
                    util.assertNever(check);
                }
            }
            return {
                status: status.value,
                value: new Date(input.data.getTime()),
            };
        }
        _addCheck(check) {
            return new ZodDate({
                ...this._def,
                checks: [...this._def.checks, check],
            });
        }
        min(minDate, message) {
            return this._addCheck({
                kind: "min",
                value: minDate.getTime(),
                message: errorUtil.toString(message),
            });
        }
        max(maxDate, message) {
            return this._addCheck({
                kind: "max",
                value: maxDate.getTime(),
                message: errorUtil.toString(message),
            });
        }
        get minDate() {
            let min = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "min") {
                    if (min === null || ch.value > min)
                        min = ch.value;
                }
            }
            return min != null ? new Date(min) : null;
        }
        get maxDate() {
            let max = null;
            for (const ch of this._def.checks) {
                if (ch.kind === "max") {
                    if (max === null || ch.value < max)
                        max = ch.value;
                }
            }
            return max != null ? new Date(max) : null;
        }
    }
    ZodDate.create = (params) => {
        return new ZodDate({
            checks: [],
            coerce: (params === null || params === void 0 ? void 0 : params.coerce) || false,
            typeName: ZodFirstPartyTypeKind.ZodDate,
            ...processCreateParams(params),
        });
    };
    class ZodSymbol extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.symbol) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.symbol,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            return OK(input.data);
        }
    }
    ZodSymbol.create = (params) => {
        return new ZodSymbol({
            typeName: ZodFirstPartyTypeKind.ZodSymbol,
            ...processCreateParams(params),
        });
    };
    class ZodUndefined extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.undefined) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.undefined,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            return OK(input.data);
        }
    }
    ZodUndefined.create = (params) => {
        return new ZodUndefined({
            typeName: ZodFirstPartyTypeKind.ZodUndefined,
            ...processCreateParams(params),
        });
    };
    class ZodNull extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.null) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.null,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            return OK(input.data);
        }
    }
    ZodNull.create = (params) => {
        return new ZodNull({
            typeName: ZodFirstPartyTypeKind.ZodNull,
            ...processCreateParams(params),
        });
    };
    class ZodAny extends ZodType {
        constructor() {
            super(...arguments);
            // to prevent instances of other classes from extending ZodAny. this causes issues with catchall in ZodObject.
            this._any = true;
        }
        _parse(input) {
            return OK(input.data);
        }
    }
    ZodAny.create = (params) => {
        return new ZodAny({
            typeName: ZodFirstPartyTypeKind.ZodAny,
            ...processCreateParams(params),
        });
    };
    class ZodUnknown extends ZodType {
        constructor() {
            super(...arguments);
            // required
            this._unknown = true;
        }
        _parse(input) {
            return OK(input.data);
        }
    }
    ZodUnknown.create = (params) => {
        return new ZodUnknown({
            typeName: ZodFirstPartyTypeKind.ZodUnknown,
            ...processCreateParams(params),
        });
    };
    class ZodNever extends ZodType {
        _parse(input) {
            const ctx = this._getOrReturnCtx(input);
            addIssueToContext(ctx, {
                code: ZodIssueCode.invalid_type,
                expected: ZodParsedType.never,
                received: ctx.parsedType,
            });
            return INVALID;
        }
    }
    ZodNever.create = (params) => {
        return new ZodNever({
            typeName: ZodFirstPartyTypeKind.ZodNever,
            ...processCreateParams(params),
        });
    };
    class ZodVoid extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.undefined) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.void,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            return OK(input.data);
        }
    }
    ZodVoid.create = (params) => {
        return new ZodVoid({
            typeName: ZodFirstPartyTypeKind.ZodVoid,
            ...processCreateParams(params),
        });
    };
    class ZodArray extends ZodType {
        _parse(input) {
            const { ctx, status } = this._processInputParams(input);
            const def = this._def;
            if (ctx.parsedType !== ZodParsedType.array) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.array,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            if (def.exactLength !== null) {
                const tooBig = ctx.data.length > def.exactLength.value;
                const tooSmall = ctx.data.length < def.exactLength.value;
                if (tooBig || tooSmall) {
                    addIssueToContext(ctx, {
                        code: tooBig ? ZodIssueCode.too_big : ZodIssueCode.too_small,
                        minimum: (tooSmall ? def.exactLength.value : undefined),
                        maximum: (tooBig ? def.exactLength.value : undefined),
                        type: "array",
                        inclusive: true,
                        exact: true,
                        message: def.exactLength.message,
                    });
                    status.dirty();
                }
            }
            if (def.minLength !== null) {
                if (ctx.data.length < def.minLength.value) {
                    addIssueToContext(ctx, {
                        code: ZodIssueCode.too_small,
                        minimum: def.minLength.value,
                        type: "array",
                        inclusive: true,
                        exact: false,
                        message: def.minLength.message,
                    });
                    status.dirty();
                }
            }
            if (def.maxLength !== null) {
                if (ctx.data.length > def.maxLength.value) {
                    addIssueToContext(ctx, {
                        code: ZodIssueCode.too_big,
                        maximum: def.maxLength.value,
                        type: "array",
                        inclusive: true,
                        exact: false,
                        message: def.maxLength.message,
                    });
                    status.dirty();
                }
            }
            if (ctx.common.async) {
                return Promise.all([...ctx.data].map((item, i) => {
                    return def.type._parseAsync(new ParseInputLazyPath(ctx, item, ctx.path, i));
                })).then((result) => {
                    return ParseStatus.mergeArray(status, result);
                });
            }
            const result = [...ctx.data].map((item, i) => {
                return def.type._parseSync(new ParseInputLazyPath(ctx, item, ctx.path, i));
            });
            return ParseStatus.mergeArray(status, result);
        }
        get element() {
            return this._def.type;
        }
        min(minLength, message) {
            return new ZodArray({
                ...this._def,
                minLength: { value: minLength, message: errorUtil.toString(message) },
            });
        }
        max(maxLength, message) {
            return new ZodArray({
                ...this._def,
                maxLength: { value: maxLength, message: errorUtil.toString(message) },
            });
        }
        length(len, message) {
            return new ZodArray({
                ...this._def,
                exactLength: { value: len, message: errorUtil.toString(message) },
            });
        }
        nonempty(message) {
            return this.min(1, message);
        }
    }
    ZodArray.create = (schema, params) => {
        return new ZodArray({
            type: schema,
            minLength: null,
            maxLength: null,
            exactLength: null,
            typeName: ZodFirstPartyTypeKind.ZodArray,
            ...processCreateParams(params),
        });
    };
    function deepPartialify(schema) {
        if (schema instanceof ZodObject) {
            const newShape = {};
            for (const key in schema.shape) {
                const fieldSchema = schema.shape[key];
                newShape[key] = ZodOptional.create(deepPartialify(fieldSchema));
            }
            return new ZodObject({
                ...schema._def,
                shape: () => newShape,
            });
        }
        else if (schema instanceof ZodArray) {
            return new ZodArray({
                ...schema._def,
                type: deepPartialify(schema.element),
            });
        }
        else if (schema instanceof ZodOptional) {
            return ZodOptional.create(deepPartialify(schema.unwrap()));
        }
        else if (schema instanceof ZodNullable) {
            return ZodNullable.create(deepPartialify(schema.unwrap()));
        }
        else if (schema instanceof ZodTuple) {
            return ZodTuple.create(schema.items.map((item) => deepPartialify(item)));
        }
        else {
            return schema;
        }
    }
    class ZodObject extends ZodType {
        constructor() {
            super(...arguments);
            this._cached = null;
            /**
             * @deprecated In most cases, this is no longer needed - unknown properties are now silently stripped.
             * If you want to pass through unknown properties, use `.passthrough()` instead.
             */
            this.nonstrict = this.passthrough;
            // extend<
            //   Augmentation extends ZodRawShape,
            //   NewOutput extends util.flatten<{
            //     [k in keyof Augmentation | keyof Output]: k extends keyof Augmentation
            //       ? Augmentation[k]["_output"]
            //       : k extends keyof Output
            //       ? Output[k]
            //       : never;
            //   }>,
            //   NewInput extends util.flatten<{
            //     [k in keyof Augmentation | keyof Input]: k extends keyof Augmentation
            //       ? Augmentation[k]["_input"]
            //       : k extends keyof Input
            //       ? Input[k]
            //       : never;
            //   }>
            // >(
            //   augmentation: Augmentation
            // ): ZodObject<
            //   extendShape<T, Augmentation>,
            //   UnknownKeys,
            //   Catchall,
            //   NewOutput,
            //   NewInput
            // > {
            //   return new ZodObject({
            //     ...this._def,
            //     shape: () => ({
            //       ...this._def.shape(),
            //       ...augmentation,
            //     }),
            //   }) as any;
            // }
            /**
             * @deprecated Use `.extend` instead
             *  */
            this.augment = this.extend;
        }
        _getCached() {
            if (this._cached !== null)
                return this._cached;
            const shape = this._def.shape();
            const keys = util.objectKeys(shape);
            return (this._cached = { shape, keys });
        }
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.object) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.object,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const { status, ctx } = this._processInputParams(input);
            const { shape, keys: shapeKeys } = this._getCached();
            const extraKeys = [];
            if (!(this._def.catchall instanceof ZodNever &&
                this._def.unknownKeys === "strip")) {
                for (const key in ctx.data) {
                    if (!shapeKeys.includes(key)) {
                        extraKeys.push(key);
                    }
                }
            }
            const pairs = [];
            for (const key of shapeKeys) {
                const keyValidator = shape[key];
                const value = ctx.data[key];
                pairs.push({
                    key: { status: "valid", value: key },
                    value: keyValidator._parse(new ParseInputLazyPath(ctx, value, ctx.path, key)),
                    alwaysSet: key in ctx.data,
                });
            }
            if (this._def.catchall instanceof ZodNever) {
                const unknownKeys = this._def.unknownKeys;
                if (unknownKeys === "passthrough") {
                    for (const key of extraKeys) {
                        pairs.push({
                            key: { status: "valid", value: key },
                            value: { status: "valid", value: ctx.data[key] },
                        });
                    }
                }
                else if (unknownKeys === "strict") {
                    if (extraKeys.length > 0) {
                        addIssueToContext(ctx, {
                            code: ZodIssueCode.unrecognized_keys,
                            keys: extraKeys,
                        });
                        status.dirty();
                    }
                }
                else if (unknownKeys === "strip") ;
                else {
                    throw new Error(`Internal ZodObject error: invalid unknownKeys value.`);
                }
            }
            else {
                // run catchall validation
                const catchall = this._def.catchall;
                for (const key of extraKeys) {
                    const value = ctx.data[key];
                    pairs.push({
                        key: { status: "valid", value: key },
                        value: catchall._parse(new ParseInputLazyPath(ctx, value, ctx.path, key) //, ctx.child(key), value, getParsedType(value)
                        ),
                        alwaysSet: key in ctx.data,
                    });
                }
            }
            if (ctx.common.async) {
                return Promise.resolve()
                    .then(async () => {
                    const syncPairs = [];
                    for (const pair of pairs) {
                        const key = await pair.key;
                        const value = await pair.value;
                        syncPairs.push({
                            key,
                            value,
                            alwaysSet: pair.alwaysSet,
                        });
                    }
                    return syncPairs;
                })
                    .then((syncPairs) => {
                    return ParseStatus.mergeObjectSync(status, syncPairs);
                });
            }
            else {
                return ParseStatus.mergeObjectSync(status, pairs);
            }
        }
        get shape() {
            return this._def.shape();
        }
        strict(message) {
            errorUtil.errToObj;
            return new ZodObject({
                ...this._def,
                unknownKeys: "strict",
                ...(message !== undefined
                    ? {
                        errorMap: (issue, ctx) => {
                            var _a, _b, _c, _d;
                            const defaultError = (_c = (_b = (_a = this._def).errorMap) === null || _b === void 0 ? void 0 : _b.call(_a, issue, ctx).message) !== null && _c !== void 0 ? _c : ctx.defaultError;
                            if (issue.code === "unrecognized_keys")
                                return {
                                    message: (_d = errorUtil.errToObj(message).message) !== null && _d !== void 0 ? _d : defaultError,
                                };
                            return {
                                message: defaultError,
                            };
                        },
                    }
                    : {}),
            });
        }
        strip() {
            return new ZodObject({
                ...this._def,
                unknownKeys: "strip",
            });
        }
        passthrough() {
            return new ZodObject({
                ...this._def,
                unknownKeys: "passthrough",
            });
        }
        // const AugmentFactory =
        //   <Def extends ZodObjectDef>(def: Def) =>
        //   <Augmentation extends ZodRawShape>(
        //     augmentation: Augmentation
        //   ): ZodObject<
        //     extendShape<ReturnType<Def["shape"]>, Augmentation>,
        //     Def["unknownKeys"],
        //     Def["catchall"]
        //   > => {
        //     return new ZodObject({
        //       ...def,
        //       shape: () => ({
        //         ...def.shape(),
        //         ...augmentation,
        //       }),
        //     }) as any;
        //   };
        extend(augmentation) {
            return new ZodObject({
                ...this._def,
                shape: () => ({
                    ...this._def.shape(),
                    ...augmentation,
                }),
            });
        }
        /**
         * Prior to zod@1.0.12 there was a bug in the
         * inferred type of merged objects. Please
         * upgrade if you are experiencing issues.
         */
        merge(merging) {
            const merged = new ZodObject({
                unknownKeys: merging._def.unknownKeys,
                catchall: merging._def.catchall,
                shape: () => ({
                    ...this._def.shape(),
                    ...merging._def.shape(),
                }),
                typeName: ZodFirstPartyTypeKind.ZodObject,
            });
            return merged;
        }
        // merge<
        //   Incoming extends AnyZodObject,
        //   Augmentation extends Incoming["shape"],
        //   NewOutput extends {
        //     [k in keyof Augmentation | keyof Output]: k extends keyof Augmentation
        //       ? Augmentation[k]["_output"]
        //       : k extends keyof Output
        //       ? Output[k]
        //       : never;
        //   },
        //   NewInput extends {
        //     [k in keyof Augmentation | keyof Input]: k extends keyof Augmentation
        //       ? Augmentation[k]["_input"]
        //       : k extends keyof Input
        //       ? Input[k]
        //       : never;
        //   }
        // >(
        //   merging: Incoming
        // ): ZodObject<
        //   extendShape<T, ReturnType<Incoming["_def"]["shape"]>>,
        //   Incoming["_def"]["unknownKeys"],
        //   Incoming["_def"]["catchall"],
        //   NewOutput,
        //   NewInput
        // > {
        //   const merged: any = new ZodObject({
        //     unknownKeys: merging._def.unknownKeys,
        //     catchall: merging._def.catchall,
        //     shape: () =>
        //       objectUtil.mergeShapes(this._def.shape(), merging._def.shape()),
        //     typeName: ZodFirstPartyTypeKind.ZodObject,
        //   }) as any;
        //   return merged;
        // }
        setKey(key, schema) {
            return this.augment({ [key]: schema });
        }
        // merge<Incoming extends AnyZodObject>(
        //   merging: Incoming
        // ): //ZodObject<T & Incoming["_shape"], UnknownKeys, Catchall> = (merging) => {
        // ZodObject<
        //   extendShape<T, ReturnType<Incoming["_def"]["shape"]>>,
        //   Incoming["_def"]["unknownKeys"],
        //   Incoming["_def"]["catchall"]
        // > {
        //   // const mergedShape = objectUtil.mergeShapes(
        //   //   this._def.shape(),
        //   //   merging._def.shape()
        //   // );
        //   const merged: any = new ZodObject({
        //     unknownKeys: merging._def.unknownKeys,
        //     catchall: merging._def.catchall,
        //     shape: () =>
        //       objectUtil.mergeShapes(this._def.shape(), merging._def.shape()),
        //     typeName: ZodFirstPartyTypeKind.ZodObject,
        //   }) as any;
        //   return merged;
        // }
        catchall(index) {
            return new ZodObject({
                ...this._def,
                catchall: index,
            });
        }
        pick(mask) {
            const shape = {};
            util.objectKeys(mask).forEach((key) => {
                if (mask[key] && this.shape[key]) {
                    shape[key] = this.shape[key];
                }
            });
            return new ZodObject({
                ...this._def,
                shape: () => shape,
            });
        }
        omit(mask) {
            const shape = {};
            util.objectKeys(this.shape).forEach((key) => {
                if (!mask[key]) {
                    shape[key] = this.shape[key];
                }
            });
            return new ZodObject({
                ...this._def,
                shape: () => shape,
            });
        }
        /**
         * @deprecated
         */
        deepPartial() {
            return deepPartialify(this);
        }
        partial(mask) {
            const newShape = {};
            util.objectKeys(this.shape).forEach((key) => {
                const fieldSchema = this.shape[key];
                if (mask && !mask[key]) {
                    newShape[key] = fieldSchema;
                }
                else {
                    newShape[key] = fieldSchema.optional();
                }
            });
            return new ZodObject({
                ...this._def,
                shape: () => newShape,
            });
        }
        required(mask) {
            const newShape = {};
            util.objectKeys(this.shape).forEach((key) => {
                if (mask && !mask[key]) {
                    newShape[key] = this.shape[key];
                }
                else {
                    const fieldSchema = this.shape[key];
                    let newField = fieldSchema;
                    while (newField instanceof ZodOptional) {
                        newField = newField._def.innerType;
                    }
                    newShape[key] = newField;
                }
            });
            return new ZodObject({
                ...this._def,
                shape: () => newShape,
            });
        }
        keyof() {
            return createZodEnum(util.objectKeys(this.shape));
        }
    }
    ZodObject.create = (shape, params) => {
        return new ZodObject({
            shape: () => shape,
            unknownKeys: "strip",
            catchall: ZodNever.create(),
            typeName: ZodFirstPartyTypeKind.ZodObject,
            ...processCreateParams(params),
        });
    };
    ZodObject.strictCreate = (shape, params) => {
        return new ZodObject({
            shape: () => shape,
            unknownKeys: "strict",
            catchall: ZodNever.create(),
            typeName: ZodFirstPartyTypeKind.ZodObject,
            ...processCreateParams(params),
        });
    };
    ZodObject.lazycreate = (shape, params) => {
        return new ZodObject({
            shape,
            unknownKeys: "strip",
            catchall: ZodNever.create(),
            typeName: ZodFirstPartyTypeKind.ZodObject,
            ...processCreateParams(params),
        });
    };
    class ZodUnion extends ZodType {
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            const options = this._def.options;
            function handleResults(results) {
                // return first issue-free validation if it exists
                for (const result of results) {
                    if (result.result.status === "valid") {
                        return result.result;
                    }
                }
                for (const result of results) {
                    if (result.result.status === "dirty") {
                        // add issues from dirty option
                        ctx.common.issues.push(...result.ctx.common.issues);
                        return result.result;
                    }
                }
                // return invalid
                const unionErrors = results.map((result) => new ZodError(result.ctx.common.issues));
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_union,
                    unionErrors,
                });
                return INVALID;
            }
            if (ctx.common.async) {
                return Promise.all(options.map(async (option) => {
                    const childCtx = {
                        ...ctx,
                        common: {
                            ...ctx.common,
                            issues: [],
                        },
                        parent: null,
                    };
                    return {
                        result: await option._parseAsync({
                            data: ctx.data,
                            path: ctx.path,
                            parent: childCtx,
                        }),
                        ctx: childCtx,
                    };
                })).then(handleResults);
            }
            else {
                let dirty = undefined;
                const issues = [];
                for (const option of options) {
                    const childCtx = {
                        ...ctx,
                        common: {
                            ...ctx.common,
                            issues: [],
                        },
                        parent: null,
                    };
                    const result = option._parseSync({
                        data: ctx.data,
                        path: ctx.path,
                        parent: childCtx,
                    });
                    if (result.status === "valid") {
                        return result;
                    }
                    else if (result.status === "dirty" && !dirty) {
                        dirty = { result, ctx: childCtx };
                    }
                    if (childCtx.common.issues.length) {
                        issues.push(childCtx.common.issues);
                    }
                }
                if (dirty) {
                    ctx.common.issues.push(...dirty.ctx.common.issues);
                    return dirty.result;
                }
                const unionErrors = issues.map((issues) => new ZodError(issues));
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_union,
                    unionErrors,
                });
                return INVALID;
            }
        }
        get options() {
            return this._def.options;
        }
    }
    ZodUnion.create = (types, params) => {
        return new ZodUnion({
            options: types,
            typeName: ZodFirstPartyTypeKind.ZodUnion,
            ...processCreateParams(params),
        });
    };
    /////////////////////////////////////////////////////
    /////////////////////////////////////////////////////
    //////////                                 //////////
    //////////      ZodDiscriminatedUnion      //////////
    //////////                                 //////////
    /////////////////////////////////////////////////////
    /////////////////////////////////////////////////////
    const getDiscriminator = (type) => {
        if (type instanceof ZodLazy) {
            return getDiscriminator(type.schema);
        }
        else if (type instanceof ZodEffects) {
            return getDiscriminator(type.innerType());
        }
        else if (type instanceof ZodLiteral) {
            return [type.value];
        }
        else if (type instanceof ZodEnum) {
            return type.options;
        }
        else if (type instanceof ZodNativeEnum) {
            // eslint-disable-next-line ban/ban
            return util.objectValues(type.enum);
        }
        else if (type instanceof ZodDefault) {
            return getDiscriminator(type._def.innerType);
        }
        else if (type instanceof ZodUndefined) {
            return [undefined];
        }
        else if (type instanceof ZodNull) {
            return [null];
        }
        else if (type instanceof ZodOptional) {
            return [undefined, ...getDiscriminator(type.unwrap())];
        }
        else if (type instanceof ZodNullable) {
            return [null, ...getDiscriminator(type.unwrap())];
        }
        else if (type instanceof ZodBranded) {
            return getDiscriminator(type.unwrap());
        }
        else if (type instanceof ZodReadonly) {
            return getDiscriminator(type.unwrap());
        }
        else if (type instanceof ZodCatch) {
            return getDiscriminator(type._def.innerType);
        }
        else {
            return [];
        }
    };
    class ZodDiscriminatedUnion extends ZodType {
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.object) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.object,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const discriminator = this.discriminator;
            const discriminatorValue = ctx.data[discriminator];
            const option = this.optionsMap.get(discriminatorValue);
            if (!option) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_union_discriminator,
                    options: Array.from(this.optionsMap.keys()),
                    path: [discriminator],
                });
                return INVALID;
            }
            if (ctx.common.async) {
                return option._parseAsync({
                    data: ctx.data,
                    path: ctx.path,
                    parent: ctx,
                });
            }
            else {
                return option._parseSync({
                    data: ctx.data,
                    path: ctx.path,
                    parent: ctx,
                });
            }
        }
        get discriminator() {
            return this._def.discriminator;
        }
        get options() {
            return this._def.options;
        }
        get optionsMap() {
            return this._def.optionsMap;
        }
        /**
         * The constructor of the discriminated union schema. Its behaviour is very similar to that of the normal z.union() constructor.
         * However, it only allows a union of objects, all of which need to share a discriminator property. This property must
         * have a different value for each object in the union.
         * @param discriminator the name of the discriminator property
         * @param types an array of object schemas
         * @param params
         */
        static create(discriminator, options, params) {
            // Get all the valid discriminator values
            const optionsMap = new Map();
            // try {
            for (const type of options) {
                const discriminatorValues = getDiscriminator(type.shape[discriminator]);
                if (!discriminatorValues.length) {
                    throw new Error(`A discriminator value for key \`${discriminator}\` could not be extracted from all schema options`);
                }
                for (const value of discriminatorValues) {
                    if (optionsMap.has(value)) {
                        throw new Error(`Discriminator property ${String(discriminator)} has duplicate value ${String(value)}`);
                    }
                    optionsMap.set(value, type);
                }
            }
            return new ZodDiscriminatedUnion({
                typeName: ZodFirstPartyTypeKind.ZodDiscriminatedUnion,
                discriminator,
                options,
                optionsMap,
                ...processCreateParams(params),
            });
        }
    }
    function mergeValues(a, b) {
        const aType = getParsedType(a);
        const bType = getParsedType(b);
        if (a === b) {
            return { valid: true, data: a };
        }
        else if (aType === ZodParsedType.object && bType === ZodParsedType.object) {
            const bKeys = util.objectKeys(b);
            const sharedKeys = util
                .objectKeys(a)
                .filter((key) => bKeys.indexOf(key) !== -1);
            const newObj = { ...a, ...b };
            for (const key of sharedKeys) {
                const sharedValue = mergeValues(a[key], b[key]);
                if (!sharedValue.valid) {
                    return { valid: false };
                }
                newObj[key] = sharedValue.data;
            }
            return { valid: true, data: newObj };
        }
        else if (aType === ZodParsedType.array && bType === ZodParsedType.array) {
            if (a.length !== b.length) {
                return { valid: false };
            }
            const newArray = [];
            for (let index = 0; index < a.length; index++) {
                const itemA = a[index];
                const itemB = b[index];
                const sharedValue = mergeValues(itemA, itemB);
                if (!sharedValue.valid) {
                    return { valid: false };
                }
                newArray.push(sharedValue.data);
            }
            return { valid: true, data: newArray };
        }
        else if (aType === ZodParsedType.date &&
            bType === ZodParsedType.date &&
            +a === +b) {
            return { valid: true, data: a };
        }
        else {
            return { valid: false };
        }
    }
    class ZodIntersection extends ZodType {
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            const handleParsed = (parsedLeft, parsedRight) => {
                if (isAborted(parsedLeft) || isAborted(parsedRight)) {
                    return INVALID;
                }
                const merged = mergeValues(parsedLeft.value, parsedRight.value);
                if (!merged.valid) {
                    addIssueToContext(ctx, {
                        code: ZodIssueCode.invalid_intersection_types,
                    });
                    return INVALID;
                }
                if (isDirty(parsedLeft) || isDirty(parsedRight)) {
                    status.dirty();
                }
                return { status: status.value, value: merged.data };
            };
            if (ctx.common.async) {
                return Promise.all([
                    this._def.left._parseAsync({
                        data: ctx.data,
                        path: ctx.path,
                        parent: ctx,
                    }),
                    this._def.right._parseAsync({
                        data: ctx.data,
                        path: ctx.path,
                        parent: ctx,
                    }),
                ]).then(([left, right]) => handleParsed(left, right));
            }
            else {
                return handleParsed(this._def.left._parseSync({
                    data: ctx.data,
                    path: ctx.path,
                    parent: ctx,
                }), this._def.right._parseSync({
                    data: ctx.data,
                    path: ctx.path,
                    parent: ctx,
                }));
            }
        }
    }
    ZodIntersection.create = (left, right, params) => {
        return new ZodIntersection({
            left: left,
            right: right,
            typeName: ZodFirstPartyTypeKind.ZodIntersection,
            ...processCreateParams(params),
        });
    };
    class ZodTuple extends ZodType {
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.array) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.array,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            if (ctx.data.length < this._def.items.length) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.too_small,
                    minimum: this._def.items.length,
                    inclusive: true,
                    exact: false,
                    type: "array",
                });
                return INVALID;
            }
            const rest = this._def.rest;
            if (!rest && ctx.data.length > this._def.items.length) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.too_big,
                    maximum: this._def.items.length,
                    inclusive: true,
                    exact: false,
                    type: "array",
                });
                status.dirty();
            }
            const items = [...ctx.data]
                .map((item, itemIndex) => {
                const schema = this._def.items[itemIndex] || this._def.rest;
                if (!schema)
                    return null;
                return schema._parse(new ParseInputLazyPath(ctx, item, ctx.path, itemIndex));
            })
                .filter((x) => !!x); // filter nulls
            if (ctx.common.async) {
                return Promise.all(items).then((results) => {
                    return ParseStatus.mergeArray(status, results);
                });
            }
            else {
                return ParseStatus.mergeArray(status, items);
            }
        }
        get items() {
            return this._def.items;
        }
        rest(rest) {
            return new ZodTuple({
                ...this._def,
                rest,
            });
        }
    }
    ZodTuple.create = (schemas, params) => {
        if (!Array.isArray(schemas)) {
            throw new Error("You must pass an array of schemas to z.tuple([ ... ])");
        }
        return new ZodTuple({
            items: schemas,
            typeName: ZodFirstPartyTypeKind.ZodTuple,
            rest: null,
            ...processCreateParams(params),
        });
    };
    class ZodRecord extends ZodType {
        get keySchema() {
            return this._def.keyType;
        }
        get valueSchema() {
            return this._def.valueType;
        }
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.object) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.object,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const pairs = [];
            const keyType = this._def.keyType;
            const valueType = this._def.valueType;
            for (const key in ctx.data) {
                pairs.push({
                    key: keyType._parse(new ParseInputLazyPath(ctx, key, ctx.path, key)),
                    value: valueType._parse(new ParseInputLazyPath(ctx, ctx.data[key], ctx.path, key)),
                    alwaysSet: key in ctx.data,
                });
            }
            if (ctx.common.async) {
                return ParseStatus.mergeObjectAsync(status, pairs);
            }
            else {
                return ParseStatus.mergeObjectSync(status, pairs);
            }
        }
        get element() {
            return this._def.valueType;
        }
        static create(first, second, third) {
            if (second instanceof ZodType) {
                return new ZodRecord({
                    keyType: first,
                    valueType: second,
                    typeName: ZodFirstPartyTypeKind.ZodRecord,
                    ...processCreateParams(third),
                });
            }
            return new ZodRecord({
                keyType: ZodString.create(),
                valueType: first,
                typeName: ZodFirstPartyTypeKind.ZodRecord,
                ...processCreateParams(second),
            });
        }
    }
    class ZodMap extends ZodType {
        get keySchema() {
            return this._def.keyType;
        }
        get valueSchema() {
            return this._def.valueType;
        }
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.map) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.map,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const keyType = this._def.keyType;
            const valueType = this._def.valueType;
            const pairs = [...ctx.data.entries()].map(([key, value], index) => {
                return {
                    key: keyType._parse(new ParseInputLazyPath(ctx, key, ctx.path, [index, "key"])),
                    value: valueType._parse(new ParseInputLazyPath(ctx, value, ctx.path, [index, "value"])),
                };
            });
            if (ctx.common.async) {
                const finalMap = new Map();
                return Promise.resolve().then(async () => {
                    for (const pair of pairs) {
                        const key = await pair.key;
                        const value = await pair.value;
                        if (key.status === "aborted" || value.status === "aborted") {
                            return INVALID;
                        }
                        if (key.status === "dirty" || value.status === "dirty") {
                            status.dirty();
                        }
                        finalMap.set(key.value, value.value);
                    }
                    return { status: status.value, value: finalMap };
                });
            }
            else {
                const finalMap = new Map();
                for (const pair of pairs) {
                    const key = pair.key;
                    const value = pair.value;
                    if (key.status === "aborted" || value.status === "aborted") {
                        return INVALID;
                    }
                    if (key.status === "dirty" || value.status === "dirty") {
                        status.dirty();
                    }
                    finalMap.set(key.value, value.value);
                }
                return { status: status.value, value: finalMap };
            }
        }
    }
    ZodMap.create = (keyType, valueType, params) => {
        return new ZodMap({
            valueType,
            keyType,
            typeName: ZodFirstPartyTypeKind.ZodMap,
            ...processCreateParams(params),
        });
    };
    class ZodSet extends ZodType {
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.set) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.set,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const def = this._def;
            if (def.minSize !== null) {
                if (ctx.data.size < def.minSize.value) {
                    addIssueToContext(ctx, {
                        code: ZodIssueCode.too_small,
                        minimum: def.minSize.value,
                        type: "set",
                        inclusive: true,
                        exact: false,
                        message: def.minSize.message,
                    });
                    status.dirty();
                }
            }
            if (def.maxSize !== null) {
                if (ctx.data.size > def.maxSize.value) {
                    addIssueToContext(ctx, {
                        code: ZodIssueCode.too_big,
                        maximum: def.maxSize.value,
                        type: "set",
                        inclusive: true,
                        exact: false,
                        message: def.maxSize.message,
                    });
                    status.dirty();
                }
            }
            const valueType = this._def.valueType;
            function finalizeSet(elements) {
                const parsedSet = new Set();
                for (const element of elements) {
                    if (element.status === "aborted")
                        return INVALID;
                    if (element.status === "dirty")
                        status.dirty();
                    parsedSet.add(element.value);
                }
                return { status: status.value, value: parsedSet };
            }
            const elements = [...ctx.data.values()].map((item, i) => valueType._parse(new ParseInputLazyPath(ctx, item, ctx.path, i)));
            if (ctx.common.async) {
                return Promise.all(elements).then((elements) => finalizeSet(elements));
            }
            else {
                return finalizeSet(elements);
            }
        }
        min(minSize, message) {
            return new ZodSet({
                ...this._def,
                minSize: { value: minSize, message: errorUtil.toString(message) },
            });
        }
        max(maxSize, message) {
            return new ZodSet({
                ...this._def,
                maxSize: { value: maxSize, message: errorUtil.toString(message) },
            });
        }
        size(size, message) {
            return this.min(size, message).max(size, message);
        }
        nonempty(message) {
            return this.min(1, message);
        }
    }
    ZodSet.create = (valueType, params) => {
        return new ZodSet({
            valueType,
            minSize: null,
            maxSize: null,
            typeName: ZodFirstPartyTypeKind.ZodSet,
            ...processCreateParams(params),
        });
    };
    class ZodFunction extends ZodType {
        constructor() {
            super(...arguments);
            this.validate = this.implement;
        }
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.function) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.function,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            function makeArgsIssue(args, error) {
                return makeIssue({
                    data: args,
                    path: ctx.path,
                    errorMaps: [
                        ctx.common.contextualErrorMap,
                        ctx.schemaErrorMap,
                        getErrorMap(),
                        errorMap,
                    ].filter((x) => !!x),
                    issueData: {
                        code: ZodIssueCode.invalid_arguments,
                        argumentsError: error,
                    },
                });
            }
            function makeReturnsIssue(returns, error) {
                return makeIssue({
                    data: returns,
                    path: ctx.path,
                    errorMaps: [
                        ctx.common.contextualErrorMap,
                        ctx.schemaErrorMap,
                        getErrorMap(),
                        errorMap,
                    ].filter((x) => !!x),
                    issueData: {
                        code: ZodIssueCode.invalid_return_type,
                        returnTypeError: error,
                    },
                });
            }
            const params = { errorMap: ctx.common.contextualErrorMap };
            const fn = ctx.data;
            if (this._def.returns instanceof ZodPromise) {
                // Would love a way to avoid disabling this rule, but we need
                // an alias (using an arrow function was what caused 2651).
                // eslint-disable-next-line @typescript-eslint/no-this-alias
                const me = this;
                return OK(async function (...args) {
                    const error = new ZodError([]);
                    const parsedArgs = await me._def.args
                        .parseAsync(args, params)
                        .catch((e) => {
                        error.addIssue(makeArgsIssue(args, e));
                        throw error;
                    });
                    const result = await Reflect.apply(fn, this, parsedArgs);
                    const parsedReturns = await me._def.returns._def.type
                        .parseAsync(result, params)
                        .catch((e) => {
                        error.addIssue(makeReturnsIssue(result, e));
                        throw error;
                    });
                    return parsedReturns;
                });
            }
            else {
                // Would love a way to avoid disabling this rule, but we need
                // an alias (using an arrow function was what caused 2651).
                // eslint-disable-next-line @typescript-eslint/no-this-alias
                const me = this;
                return OK(function (...args) {
                    const parsedArgs = me._def.args.safeParse(args, params);
                    if (!parsedArgs.success) {
                        throw new ZodError([makeArgsIssue(args, parsedArgs.error)]);
                    }
                    const result = Reflect.apply(fn, this, parsedArgs.data);
                    const parsedReturns = me._def.returns.safeParse(result, params);
                    if (!parsedReturns.success) {
                        throw new ZodError([makeReturnsIssue(result, parsedReturns.error)]);
                    }
                    return parsedReturns.data;
                });
            }
        }
        parameters() {
            return this._def.args;
        }
        returnType() {
            return this._def.returns;
        }
        args(...items) {
            return new ZodFunction({
                ...this._def,
                args: ZodTuple.create(items).rest(ZodUnknown.create()),
            });
        }
        returns(returnType) {
            return new ZodFunction({
                ...this._def,
                returns: returnType,
            });
        }
        implement(func) {
            const validatedFunc = this.parse(func);
            return validatedFunc;
        }
        strictImplement(func) {
            const validatedFunc = this.parse(func);
            return validatedFunc;
        }
        static create(args, returns, params) {
            return new ZodFunction({
                args: (args
                    ? args
                    : ZodTuple.create([]).rest(ZodUnknown.create())),
                returns: returns || ZodUnknown.create(),
                typeName: ZodFirstPartyTypeKind.ZodFunction,
                ...processCreateParams(params),
            });
        }
    }
    class ZodLazy extends ZodType {
        get schema() {
            return this._def.getter();
        }
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            const lazySchema = this._def.getter();
            return lazySchema._parse({ data: ctx.data, path: ctx.path, parent: ctx });
        }
    }
    ZodLazy.create = (getter, params) => {
        return new ZodLazy({
            getter: getter,
            typeName: ZodFirstPartyTypeKind.ZodLazy,
            ...processCreateParams(params),
        });
    };
    class ZodLiteral extends ZodType {
        _parse(input) {
            if (input.data !== this._def.value) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    received: ctx.data,
                    code: ZodIssueCode.invalid_literal,
                    expected: this._def.value,
                });
                return INVALID;
            }
            return { status: "valid", value: input.data };
        }
        get value() {
            return this._def.value;
        }
    }
    ZodLiteral.create = (value, params) => {
        return new ZodLiteral({
            value: value,
            typeName: ZodFirstPartyTypeKind.ZodLiteral,
            ...processCreateParams(params),
        });
    };
    function createZodEnum(values, params) {
        return new ZodEnum({
            values,
            typeName: ZodFirstPartyTypeKind.ZodEnum,
            ...processCreateParams(params),
        });
    }
    class ZodEnum extends ZodType {
        constructor() {
            super(...arguments);
            _ZodEnum_cache.set(this, void 0);
        }
        _parse(input) {
            if (typeof input.data !== "string") {
                const ctx = this._getOrReturnCtx(input);
                const expectedValues = this._def.values;
                addIssueToContext(ctx, {
                    expected: util.joinValues(expectedValues),
                    received: ctx.parsedType,
                    code: ZodIssueCode.invalid_type,
                });
                return INVALID;
            }
            if (!__classPrivateFieldGet(this, _ZodEnum_cache)) {
                __classPrivateFieldSet(this, _ZodEnum_cache, new Set(this._def.values));
            }
            if (!__classPrivateFieldGet(this, _ZodEnum_cache).has(input.data)) {
                const ctx = this._getOrReturnCtx(input);
                const expectedValues = this._def.values;
                addIssueToContext(ctx, {
                    received: ctx.data,
                    code: ZodIssueCode.invalid_enum_value,
                    options: expectedValues,
                });
                return INVALID;
            }
            return OK(input.data);
        }
        get options() {
            return this._def.values;
        }
        get enum() {
            const enumValues = {};
            for (const val of this._def.values) {
                enumValues[val] = val;
            }
            return enumValues;
        }
        get Values() {
            const enumValues = {};
            for (const val of this._def.values) {
                enumValues[val] = val;
            }
            return enumValues;
        }
        get Enum() {
            const enumValues = {};
            for (const val of this._def.values) {
                enumValues[val] = val;
            }
            return enumValues;
        }
        extract(values, newDef = this._def) {
            return ZodEnum.create(values, {
                ...this._def,
                ...newDef,
            });
        }
        exclude(values, newDef = this._def) {
            return ZodEnum.create(this.options.filter((opt) => !values.includes(opt)), {
                ...this._def,
                ...newDef,
            });
        }
    }
    _ZodEnum_cache = new WeakMap();
    ZodEnum.create = createZodEnum;
    class ZodNativeEnum extends ZodType {
        constructor() {
            super(...arguments);
            _ZodNativeEnum_cache.set(this, void 0);
        }
        _parse(input) {
            const nativeEnumValues = util.getValidEnumValues(this._def.values);
            const ctx = this._getOrReturnCtx(input);
            if (ctx.parsedType !== ZodParsedType.string &&
                ctx.parsedType !== ZodParsedType.number) {
                const expectedValues = util.objectValues(nativeEnumValues);
                addIssueToContext(ctx, {
                    expected: util.joinValues(expectedValues),
                    received: ctx.parsedType,
                    code: ZodIssueCode.invalid_type,
                });
                return INVALID;
            }
            if (!__classPrivateFieldGet(this, _ZodNativeEnum_cache)) {
                __classPrivateFieldSet(this, _ZodNativeEnum_cache, new Set(util.getValidEnumValues(this._def.values)));
            }
            if (!__classPrivateFieldGet(this, _ZodNativeEnum_cache).has(input.data)) {
                const expectedValues = util.objectValues(nativeEnumValues);
                addIssueToContext(ctx, {
                    received: ctx.data,
                    code: ZodIssueCode.invalid_enum_value,
                    options: expectedValues,
                });
                return INVALID;
            }
            return OK(input.data);
        }
        get enum() {
            return this._def.values;
        }
    }
    _ZodNativeEnum_cache = new WeakMap();
    ZodNativeEnum.create = (values, params) => {
        return new ZodNativeEnum({
            values: values,
            typeName: ZodFirstPartyTypeKind.ZodNativeEnum,
            ...processCreateParams(params),
        });
    };
    class ZodPromise extends ZodType {
        unwrap() {
            return this._def.type;
        }
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            if (ctx.parsedType !== ZodParsedType.promise &&
                ctx.common.async === false) {
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.promise,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            const promisified = ctx.parsedType === ZodParsedType.promise
                ? ctx.data
                : Promise.resolve(ctx.data);
            return OK(promisified.then((data) => {
                return this._def.type.parseAsync(data, {
                    path: ctx.path,
                    errorMap: ctx.common.contextualErrorMap,
                });
            }));
        }
    }
    ZodPromise.create = (schema, params) => {
        return new ZodPromise({
            type: schema,
            typeName: ZodFirstPartyTypeKind.ZodPromise,
            ...processCreateParams(params),
        });
    };
    class ZodEffects extends ZodType {
        innerType() {
            return this._def.schema;
        }
        sourceType() {
            return this._def.schema._def.typeName === ZodFirstPartyTypeKind.ZodEffects
                ? this._def.schema.sourceType()
                : this._def.schema;
        }
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            const effect = this._def.effect || null;
            const checkCtx = {
                addIssue: (arg) => {
                    addIssueToContext(ctx, arg);
                    if (arg.fatal) {
                        status.abort();
                    }
                    else {
                        status.dirty();
                    }
                },
                get path() {
                    return ctx.path;
                },
            };
            checkCtx.addIssue = checkCtx.addIssue.bind(checkCtx);
            if (effect.type === "preprocess") {
                const processed = effect.transform(ctx.data, checkCtx);
                if (ctx.common.async) {
                    return Promise.resolve(processed).then(async (processed) => {
                        if (status.value === "aborted")
                            return INVALID;
                        const result = await this._def.schema._parseAsync({
                            data: processed,
                            path: ctx.path,
                            parent: ctx,
                        });
                        if (result.status === "aborted")
                            return INVALID;
                        if (result.status === "dirty")
                            return DIRTY(result.value);
                        if (status.value === "dirty")
                            return DIRTY(result.value);
                        return result;
                    });
                }
                else {
                    if (status.value === "aborted")
                        return INVALID;
                    const result = this._def.schema._parseSync({
                        data: processed,
                        path: ctx.path,
                        parent: ctx,
                    });
                    if (result.status === "aborted")
                        return INVALID;
                    if (result.status === "dirty")
                        return DIRTY(result.value);
                    if (status.value === "dirty")
                        return DIRTY(result.value);
                    return result;
                }
            }
            if (effect.type === "refinement") {
                const executeRefinement = (acc) => {
                    const result = effect.refinement(acc, checkCtx);
                    if (ctx.common.async) {
                        return Promise.resolve(result);
                    }
                    if (result instanceof Promise) {
                        throw new Error("Async refinement encountered during synchronous parse operation. Use .parseAsync instead.");
                    }
                    return acc;
                };
                if (ctx.common.async === false) {
                    const inner = this._def.schema._parseSync({
                        data: ctx.data,
                        path: ctx.path,
                        parent: ctx,
                    });
                    if (inner.status === "aborted")
                        return INVALID;
                    if (inner.status === "dirty")
                        status.dirty();
                    // return value is ignored
                    executeRefinement(inner.value);
                    return { status: status.value, value: inner.value };
                }
                else {
                    return this._def.schema
                        ._parseAsync({ data: ctx.data, path: ctx.path, parent: ctx })
                        .then((inner) => {
                        if (inner.status === "aborted")
                            return INVALID;
                        if (inner.status === "dirty")
                            status.dirty();
                        return executeRefinement(inner.value).then(() => {
                            return { status: status.value, value: inner.value };
                        });
                    });
                }
            }
            if (effect.type === "transform") {
                if (ctx.common.async === false) {
                    const base = this._def.schema._parseSync({
                        data: ctx.data,
                        path: ctx.path,
                        parent: ctx,
                    });
                    if (!isValid(base))
                        return base;
                    const result = effect.transform(base.value, checkCtx);
                    if (result instanceof Promise) {
                        throw new Error(`Asynchronous transform encountered during synchronous parse operation. Use .parseAsync instead.`);
                    }
                    return { status: status.value, value: result };
                }
                else {
                    return this._def.schema
                        ._parseAsync({ data: ctx.data, path: ctx.path, parent: ctx })
                        .then((base) => {
                        if (!isValid(base))
                            return base;
                        return Promise.resolve(effect.transform(base.value, checkCtx)).then((result) => ({ status: status.value, value: result }));
                    });
                }
            }
            util.assertNever(effect);
        }
    }
    ZodEffects.create = (schema, effect, params) => {
        return new ZodEffects({
            schema,
            typeName: ZodFirstPartyTypeKind.ZodEffects,
            effect,
            ...processCreateParams(params),
        });
    };
    ZodEffects.createWithPreprocess = (preprocess, schema, params) => {
        return new ZodEffects({
            schema,
            effect: { type: "preprocess", transform: preprocess },
            typeName: ZodFirstPartyTypeKind.ZodEffects,
            ...processCreateParams(params),
        });
    };
    class ZodOptional extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType === ZodParsedType.undefined) {
                return OK(undefined);
            }
            return this._def.innerType._parse(input);
        }
        unwrap() {
            return this._def.innerType;
        }
    }
    ZodOptional.create = (type, params) => {
        return new ZodOptional({
            innerType: type,
            typeName: ZodFirstPartyTypeKind.ZodOptional,
            ...processCreateParams(params),
        });
    };
    class ZodNullable extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType === ZodParsedType.null) {
                return OK(null);
            }
            return this._def.innerType._parse(input);
        }
        unwrap() {
            return this._def.innerType;
        }
    }
    ZodNullable.create = (type, params) => {
        return new ZodNullable({
            innerType: type,
            typeName: ZodFirstPartyTypeKind.ZodNullable,
            ...processCreateParams(params),
        });
    };
    class ZodDefault extends ZodType {
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            let data = ctx.data;
            if (ctx.parsedType === ZodParsedType.undefined) {
                data = this._def.defaultValue();
            }
            return this._def.innerType._parse({
                data,
                path: ctx.path,
                parent: ctx,
            });
        }
        removeDefault() {
            return this._def.innerType;
        }
    }
    ZodDefault.create = (type, params) => {
        return new ZodDefault({
            innerType: type,
            typeName: ZodFirstPartyTypeKind.ZodDefault,
            defaultValue: typeof params.default === "function"
                ? params.default
                : () => params.default,
            ...processCreateParams(params),
        });
    };
    class ZodCatch extends ZodType {
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            // newCtx is used to not collect issues from inner types in ctx
            const newCtx = {
                ...ctx,
                common: {
                    ...ctx.common,
                    issues: [],
                },
            };
            const result = this._def.innerType._parse({
                data: newCtx.data,
                path: newCtx.path,
                parent: {
                    ...newCtx,
                },
            });
            if (isAsync(result)) {
                return result.then((result) => {
                    return {
                        status: "valid",
                        value: result.status === "valid"
                            ? result.value
                            : this._def.catchValue({
                                get error() {
                                    return new ZodError(newCtx.common.issues);
                                },
                                input: newCtx.data,
                            }),
                    };
                });
            }
            else {
                return {
                    status: "valid",
                    value: result.status === "valid"
                        ? result.value
                        : this._def.catchValue({
                            get error() {
                                return new ZodError(newCtx.common.issues);
                            },
                            input: newCtx.data,
                        }),
                };
            }
        }
        removeCatch() {
            return this._def.innerType;
        }
    }
    ZodCatch.create = (type, params) => {
        return new ZodCatch({
            innerType: type,
            typeName: ZodFirstPartyTypeKind.ZodCatch,
            catchValue: typeof params.catch === "function" ? params.catch : () => params.catch,
            ...processCreateParams(params),
        });
    };
    class ZodNaN extends ZodType {
        _parse(input) {
            const parsedType = this._getType(input);
            if (parsedType !== ZodParsedType.nan) {
                const ctx = this._getOrReturnCtx(input);
                addIssueToContext(ctx, {
                    code: ZodIssueCode.invalid_type,
                    expected: ZodParsedType.nan,
                    received: ctx.parsedType,
                });
                return INVALID;
            }
            return { status: "valid", value: input.data };
        }
    }
    ZodNaN.create = (params) => {
        return new ZodNaN({
            typeName: ZodFirstPartyTypeKind.ZodNaN,
            ...processCreateParams(params),
        });
    };
    const BRAND = Symbol("zod_brand");
    class ZodBranded extends ZodType {
        _parse(input) {
            const { ctx } = this._processInputParams(input);
            const data = ctx.data;
            return this._def.type._parse({
                data,
                path: ctx.path,
                parent: ctx,
            });
        }
        unwrap() {
            return this._def.type;
        }
    }
    class ZodPipeline extends ZodType {
        _parse(input) {
            const { status, ctx } = this._processInputParams(input);
            if (ctx.common.async) {
                const handleAsync = async () => {
                    const inResult = await this._def.in._parseAsync({
                        data: ctx.data,
                        path: ctx.path,
                        parent: ctx,
                    });
                    if (inResult.status === "aborted")
                        return INVALID;
                    if (inResult.status === "dirty") {
                        status.dirty();
                        return DIRTY(inResult.value);
                    }
                    else {
                        return this._def.out._parseAsync({
                            data: inResult.value,
                            path: ctx.path,
                            parent: ctx,
                        });
                    }
                };
                return handleAsync();
            }
            else {
                const inResult = this._def.in._parseSync({
                    data: ctx.data,
                    path: ctx.path,
                    parent: ctx,
                });
                if (inResult.status === "aborted")
                    return INVALID;
                if (inResult.status === "dirty") {
                    status.dirty();
                    return {
                        status: "dirty",
                        value: inResult.value,
                    };
                }
                else {
                    return this._def.out._parseSync({
                        data: inResult.value,
                        path: ctx.path,
                        parent: ctx,
                    });
                }
            }
        }
        static create(a, b) {
            return new ZodPipeline({
                in: a,
                out: b,
                typeName: ZodFirstPartyTypeKind.ZodPipeline,
            });
        }
    }
    class ZodReadonly extends ZodType {
        _parse(input) {
            const result = this._def.innerType._parse(input);
            const freeze = (data) => {
                if (isValid(data)) {
                    data.value = Object.freeze(data.value);
                }
                return data;
            };
            return isAsync(result)
                ? result.then((data) => freeze(data))
                : freeze(result);
        }
        unwrap() {
            return this._def.innerType;
        }
    }
    ZodReadonly.create = (type, params) => {
        return new ZodReadonly({
            innerType: type,
            typeName: ZodFirstPartyTypeKind.ZodReadonly,
            ...processCreateParams(params),
        });
    };
    function custom(check, params = {}, 
    /**
     * @deprecated
     *
     * Pass `fatal` into the params object instead:
     *
     * ```ts
     * z.string().custom((val) => val.length > 5, { fatal: false })
     * ```
     *
     */
    fatal) {
        if (check)
            return ZodAny.create().superRefine((data, ctx) => {
                var _a, _b;
                if (!check(data)) {
                    const p = typeof params === "function"
                        ? params(data)
                        : typeof params === "string"
                            ? { message: params }
                            : params;
                    const _fatal = (_b = (_a = p.fatal) !== null && _a !== void 0 ? _a : fatal) !== null && _b !== void 0 ? _b : true;
                    const p2 = typeof p === "string" ? { message: p } : p;
                    ctx.addIssue({ code: "custom", ...p2, fatal: _fatal });
                }
            });
        return ZodAny.create();
    }
    const late = {
        object: ZodObject.lazycreate,
    };
    var ZodFirstPartyTypeKind;
    (function (ZodFirstPartyTypeKind) {
        ZodFirstPartyTypeKind["ZodString"] = "ZodString";
        ZodFirstPartyTypeKind["ZodNumber"] = "ZodNumber";
        ZodFirstPartyTypeKind["ZodNaN"] = "ZodNaN";
        ZodFirstPartyTypeKind["ZodBigInt"] = "ZodBigInt";
        ZodFirstPartyTypeKind["ZodBoolean"] = "ZodBoolean";
        ZodFirstPartyTypeKind["ZodDate"] = "ZodDate";
        ZodFirstPartyTypeKind["ZodSymbol"] = "ZodSymbol";
        ZodFirstPartyTypeKind["ZodUndefined"] = "ZodUndefined";
        ZodFirstPartyTypeKind["ZodNull"] = "ZodNull";
        ZodFirstPartyTypeKind["ZodAny"] = "ZodAny";
        ZodFirstPartyTypeKind["ZodUnknown"] = "ZodUnknown";
        ZodFirstPartyTypeKind["ZodNever"] = "ZodNever";
        ZodFirstPartyTypeKind["ZodVoid"] = "ZodVoid";
        ZodFirstPartyTypeKind["ZodArray"] = "ZodArray";
        ZodFirstPartyTypeKind["ZodObject"] = "ZodObject";
        ZodFirstPartyTypeKind["ZodUnion"] = "ZodUnion";
        ZodFirstPartyTypeKind["ZodDiscriminatedUnion"] = "ZodDiscriminatedUnion";
        ZodFirstPartyTypeKind["ZodIntersection"] = "ZodIntersection";
        ZodFirstPartyTypeKind["ZodTuple"] = "ZodTuple";
        ZodFirstPartyTypeKind["ZodRecord"] = "ZodRecord";
        ZodFirstPartyTypeKind["ZodMap"] = "ZodMap";
        ZodFirstPartyTypeKind["ZodSet"] = "ZodSet";
        ZodFirstPartyTypeKind["ZodFunction"] = "ZodFunction";
        ZodFirstPartyTypeKind["ZodLazy"] = "ZodLazy";
        ZodFirstPartyTypeKind["ZodLiteral"] = "ZodLiteral";
        ZodFirstPartyTypeKind["ZodEnum"] = "ZodEnum";
        ZodFirstPartyTypeKind["ZodEffects"] = "ZodEffects";
        ZodFirstPartyTypeKind["ZodNativeEnum"] = "ZodNativeEnum";
        ZodFirstPartyTypeKind["ZodOptional"] = "ZodOptional";
        ZodFirstPartyTypeKind["ZodNullable"] = "ZodNullable";
        ZodFirstPartyTypeKind["ZodDefault"] = "ZodDefault";
        ZodFirstPartyTypeKind["ZodCatch"] = "ZodCatch";
        ZodFirstPartyTypeKind["ZodPromise"] = "ZodPromise";
        ZodFirstPartyTypeKind["ZodBranded"] = "ZodBranded";
        ZodFirstPartyTypeKind["ZodPipeline"] = "ZodPipeline";
        ZodFirstPartyTypeKind["ZodReadonly"] = "ZodReadonly";
    })(ZodFirstPartyTypeKind || (ZodFirstPartyTypeKind = {}));
    const instanceOfType = (
    // const instanceOfType = <T extends new (...args: any[]) => any>(
    cls, params = {
        message: `Input not instance of ${cls.name}`,
    }) => custom((data) => data instanceof cls, params);
    const stringType = ZodString.create;
    const numberType = ZodNumber.create;
    const nanType = ZodNaN.create;
    const bigIntType = ZodBigInt.create;
    const booleanType = ZodBoolean.create;
    const dateType = ZodDate.create;
    const symbolType = ZodSymbol.create;
    const undefinedType = ZodUndefined.create;
    const nullType = ZodNull.create;
    const anyType = ZodAny.create;
    const unknownType = ZodUnknown.create;
    const neverType = ZodNever.create;
    const voidType = ZodVoid.create;
    const arrayType = ZodArray.create;
    const objectType = ZodObject.create;
    const strictObjectType = ZodObject.strictCreate;
    const unionType = ZodUnion.create;
    const discriminatedUnionType = ZodDiscriminatedUnion.create;
    const intersectionType = ZodIntersection.create;
    const tupleType = ZodTuple.create;
    const recordType = ZodRecord.create;
    const mapType = ZodMap.create;
    const setType = ZodSet.create;
    const functionType = ZodFunction.create;
    const lazyType = ZodLazy.create;
    const literalType = ZodLiteral.create;
    const enumType = ZodEnum.create;
    const nativeEnumType = ZodNativeEnum.create;
    const promiseType = ZodPromise.create;
    const effectsType = ZodEffects.create;
    const optionalType = ZodOptional.create;
    const nullableType = ZodNullable.create;
    const preprocessType = ZodEffects.createWithPreprocess;
    const pipelineType = ZodPipeline.create;
    const ostring = () => stringType().optional();
    const onumber = () => numberType().optional();
    const oboolean = () => booleanType().optional();
    const coerce = {
        string: ((arg) => ZodString.create({ ...arg, coerce: true })),
        number: ((arg) => ZodNumber.create({ ...arg, coerce: true })),
        boolean: ((arg) => ZodBoolean.create({
            ...arg,
            coerce: true,
        })),
        bigint: ((arg) => ZodBigInt.create({ ...arg, coerce: true })),
        date: ((arg) => ZodDate.create({ ...arg, coerce: true })),
    };
    const NEVER = INVALID;

    var z = /*#__PURE__*/Object.freeze({
        __proto__: null,
        defaultErrorMap: errorMap,
        setErrorMap: setErrorMap,
        getErrorMap: getErrorMap,
        makeIssue: makeIssue,
        EMPTY_PATH: EMPTY_PATH,
        addIssueToContext: addIssueToContext,
        ParseStatus: ParseStatus,
        INVALID: INVALID,
        DIRTY: DIRTY,
        OK: OK,
        isAborted: isAborted,
        isDirty: isDirty,
        isValid: isValid,
        isAsync: isAsync,
        get util () { return util; },
        get objectUtil () { return objectUtil; },
        ZodParsedType: ZodParsedType,
        getParsedType: getParsedType,
        ZodType: ZodType,
        datetimeRegex: datetimeRegex,
        ZodString: ZodString,
        ZodNumber: ZodNumber,
        ZodBigInt: ZodBigInt,
        ZodBoolean: ZodBoolean,
        ZodDate: ZodDate,
        ZodSymbol: ZodSymbol,
        ZodUndefined: ZodUndefined,
        ZodNull: ZodNull,
        ZodAny: ZodAny,
        ZodUnknown: ZodUnknown,
        ZodNever: ZodNever,
        ZodVoid: ZodVoid,
        ZodArray: ZodArray,
        ZodObject: ZodObject,
        ZodUnion: ZodUnion,
        ZodDiscriminatedUnion: ZodDiscriminatedUnion,
        ZodIntersection: ZodIntersection,
        ZodTuple: ZodTuple,
        ZodRecord: ZodRecord,
        ZodMap: ZodMap,
        ZodSet: ZodSet,
        ZodFunction: ZodFunction,
        ZodLazy: ZodLazy,
        ZodLiteral: ZodLiteral,
        ZodEnum: ZodEnum,
        ZodNativeEnum: ZodNativeEnum,
        ZodPromise: ZodPromise,
        ZodEffects: ZodEffects,
        ZodTransformer: ZodEffects,
        ZodOptional: ZodOptional,
        ZodNullable: ZodNullable,
        ZodDefault: ZodDefault,
        ZodCatch: ZodCatch,
        ZodNaN: ZodNaN,
        BRAND: BRAND,
        ZodBranded: ZodBranded,
        ZodPipeline: ZodPipeline,
        ZodReadonly: ZodReadonly,
        custom: custom,
        Schema: ZodType,
        ZodSchema: ZodType,
        late: late,
        get ZodFirstPartyTypeKind () { return ZodFirstPartyTypeKind; },
        coerce: coerce,
        any: anyType,
        array: arrayType,
        bigint: bigIntType,
        boolean: booleanType,
        date: dateType,
        discriminatedUnion: discriminatedUnionType,
        effect: effectsType,
        'enum': enumType,
        'function': functionType,
        'instanceof': instanceOfType,
        intersection: intersectionType,
        lazy: lazyType,
        literal: literalType,
        map: mapType,
        nan: nanType,
        nativeEnum: nativeEnumType,
        never: neverType,
        'null': nullType,
        nullable: nullableType,
        number: numberType,
        object: objectType,
        oboolean: oboolean,
        onumber: onumber,
        optional: optionalType,
        ostring: ostring,
        pipeline: pipelineType,
        preprocess: preprocessType,
        promise: promiseType,
        record: recordType,
        set: setType,
        strictObject: strictObjectType,
        string: stringType,
        symbol: symbolType,
        transformer: effectsType,
        tuple: tupleType,
        'undefined': undefinedType,
        union: unionType,
        unknown: unknownType,
        'void': voidType,
        NEVER: NEVER,
        ZodIssueCode: ZodIssueCode,
        quotelessJson: quotelessJson,
        ZodError: ZodError
    });

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * THIS FILE IS AUTOGENERATED by cddlconv 0.1.5.
     * Run `node tools/generate-bidi-types.mjs` to regenerate.
     * @see https://github.com/w3c/webdriver-bidi/blob/master/index.bs
     */
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-nocheck Some types may be circular.
    var Bluetooth$1;
    (function (Bluetooth) {
        Bluetooth.RequestDeviceSchema = z.lazy(() => z.string());
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.RequestDeviceInfoSchema = z.lazy(() => z.object({
            id: Bluetooth.RequestDeviceSchema,
            name: z.union([z.string(), z.null()]),
        }));
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.RequestDevicePromptSchema = z.lazy(() => z.string());
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.HandleRequestDevicePromptSchema = z.lazy(() => z.object({
            method: z.literal('bluetooth.handleRequestDevicePrompt'),
            params: Bluetooth.HandleRequestDevicePromptParametersSchema,
        }));
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.HandleRequestDevicePromptParametersSchema = z.lazy(() => z
            .object({
            context: z.string(),
            prompt: Bluetooth.RequestDevicePromptSchema,
        })
            .and(z.union([
            Bluetooth.HandleRequestDevicePromptAcceptParametersSchema,
            Bluetooth.HandleRequestDevicePromptCancelParametersSchema,
        ])));
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.HandleRequestDevicePromptAcceptParametersSchema = z.lazy(() => z.object({
            accept: z.literal(true),
            device: Bluetooth.RequestDeviceSchema,
        }));
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.HandleRequestDevicePromptCancelParametersSchema = z.lazy(() => z.object({
            accept: z.literal(false),
        }));
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.RequestDevicePromptUpdatedSchema = z.lazy(() => z.object({
            method: z.literal('bluetooth.requestDevicePromptUpdated'),
            params: Bluetooth.RequestDevicePromptUpdatedParametersSchema,
        }));
    })(Bluetooth$1 || (Bluetooth$1 = {}));
    (function (Bluetooth) {
        Bluetooth.RequestDevicePromptUpdatedParametersSchema = z.lazy(() => z.object({
            context: z.string(),
            prompt: Bluetooth.RequestDevicePromptSchema,
            devices: z.array(Bluetooth.RequestDeviceInfoSchema),
        }));
    })(Bluetooth$1 || (Bluetooth$1 = {}));

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * THIS FILE IS AUTOGENERATED by cddlconv 0.1.5.
     * Run `node tools/generate-bidi-types.mjs` to regenerate.
     * @see https://github.com/w3c/webdriver-bidi/blob/master/index.bs
     */
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-nocheck Some types may be circular.
    z.lazy(() => Permissions$1.SetPermissionSchema);
    var Permissions$1;
    (function (Permissions) {
        Permissions.PermissionDescriptorSchema = z.lazy(() => z.object({
            name: z.string(),
        }));
    })(Permissions$1 || (Permissions$1 = {}));
    (function (Permissions) {
        Permissions.PermissionStateSchema = z.lazy(() => z.enum(['granted', 'denied', 'prompt']));
    })(Permissions$1 || (Permissions$1 = {}));
    (function (Permissions) {
        Permissions.SetPermissionSchema = z.lazy(() => z.object({
            method: z.literal('permissions.setPermission'),
            params: Permissions.SetPermissionParametersSchema,
        }));
    })(Permissions$1 || (Permissions$1 = {}));
    (function (Permissions) {
        Permissions.SetPermissionParametersSchema = z.lazy(() => z.object({
            descriptor: Permissions.PermissionDescriptorSchema,
            state: Permissions.PermissionStateSchema,
            origin: z.string(),
            userContext: z.string().optional(),
        }));
    })(Permissions$1 || (Permissions$1 = {}));

    /**
     * Copyright 2024 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * THIS FILE IS AUTOGENERATED by cddlconv 0.1.5.
     * Run `node tools/generate-bidi-types.mjs` to regenerate.
     * @see https://github.com/w3c/webdriver-bidi/blob/master/index.bs
     */
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-nocheck Some types may be circular.
    const EventSchema = z.lazy(() => z
        .object({
        type: z.literal('event'),
    })
        .and(EventDataSchema)
        .and(ExtensibleSchema));
    z.lazy(() => z
        .object({
        id: JsUintSchema,
    })
        .and(CommandDataSchema)
        .and(ExtensibleSchema));
    const CommandResponseSchema = z.lazy(() => z
        .object({
        type: z.literal('success'),
        id: JsUintSchema,
        result: ResultDataSchema,
    })
        .and(ExtensibleSchema));
    const EventDataSchema = z.lazy(() => z.union([
        BrowsingContextEventSchema,
        LogEventSchema,
        NetworkEventSchema,
        ScriptEventSchema,
    ]));
    const CommandDataSchema = z.lazy(() => z.union([
        BrowserCommandSchema,
        BrowsingContextCommandSchema,
        InputCommandSchema,
        NetworkCommandSchema,
        ScriptCommandSchema,
        SessionCommandSchema,
        StorageCommandSchema,
    ]));
    const ResultDataSchema = z.lazy(() => z.union([
        BrowsingContextResultSchema,
        EmptyResultSchema,
        NetworkResultSchema,
        ScriptResultSchema,
        SessionResultSchema,
        StorageResultSchema,
    ]));
    const EmptyParamsSchema = z.lazy(() => ExtensibleSchema);
    z.lazy(() => z.union([CommandResponseSchema, ErrorResponseSchema, EventSchema]));
    const ErrorResponseSchema = z.lazy(() => z
        .object({
        type: z.literal('error'),
        id: z.union([JsUintSchema, z.null()]),
        error: ErrorCodeSchema,
        message: z.string(),
        stacktrace: z.string().optional(),
    })
        .and(ExtensibleSchema));
    const EmptyResultSchema = z.lazy(() => ExtensibleSchema);
    const ExtensibleSchema = z.lazy(() => z.record(z.string(), z.any()));
    const JsIntSchema = z
        .number()
        .int()
        .gte(-9007199254740991)
        .lte(9007199254740991);
    const JsUintSchema = z
        .number()
        .int()
        .nonnegative()
        .gte(0)
        .lte(9007199254740991);
    const ErrorCodeSchema = z.lazy(() => z.enum([
        'invalid argument',
        'invalid selector',
        'invalid session id',
        'move target out of bounds',
        'no such alert',
        'no such element',
        'no such frame',
        'no such handle',
        'no such history entry',
        'no such intercept',
        'no such node',
        'no such request',
        'no such script',
        'no such storage partition',
        'no such user context',
        'session not created',
        'unable to capture screen',
        'unable to close browser',
        'unable to set cookie',
        'unable to set file input',
        'underspecified storage partition',
        'unknown command',
        'unknown error',
        'unsupported operation',
    ]));
    const SessionCommandSchema = z.lazy(() => z.union([
        Session$1.EndSchema,
        Session$1.NewSchema,
        Session$1.StatusSchema,
        Session$1.SubscribeSchema,
        Session$1.UnsubscribeSchema,
    ]));
    var Session$1;
    (function (Session) {
        Session.ProxyConfigurationSchema = z.lazy(() => z.union([
            Session.AutodetectProxyConfigurationSchema,
            Session.DirectProxyConfigurationSchema,
            Session.ManualProxyConfigurationSchema,
            Session.PacProxyConfigurationSchema,
            Session.SystemProxyConfigurationSchema,
            z.object({}),
        ]));
    })(Session$1 || (Session$1 = {}));
    const SessionResultSchema = z.lazy(() => z.union([Session$1.NewResultSchema, Session$1.StatusResultSchema]));
    (function (Session) {
        Session.CapabilitiesRequestSchema = z.lazy(() => z.object({
            alwaysMatch: Session.CapabilityRequestSchema.optional(),
            firstMatch: z.array(Session.CapabilityRequestSchema).optional(),
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.CapabilityRequestSchema = z.lazy(() => z
            .object({
            acceptInsecureCerts: z.boolean().optional(),
            browserName: z.string().optional(),
            browserVersion: z.string().optional(),
            platformName: z.string().optional(),
            proxy: Session.ProxyConfigurationSchema.optional(),
            unhandledPromptBehavior: Session.UserPromptHandlerSchema.optional(),
        })
            .and(ExtensibleSchema));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.AutodetectProxyConfigurationSchema = z.lazy(() => z
            .object({
            proxyType: z.literal('autodetect'),
        })
            .and(ExtensibleSchema));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.DirectProxyConfigurationSchema = z.lazy(() => z
            .object({
            proxyType: z.literal('direct'),
        })
            .and(ExtensibleSchema));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.ManualProxyConfigurationSchema = z.lazy(() => z
            .object({
            proxyType: z.literal('manual'),
            ftpProxy: z.string().optional(),
            httpProxy: z.string().optional(),
            sslProxy: z.string().optional(),
        })
            .and(Session.SocksProxyConfigurationSchema.or(z.object({})))
            .and(z.object({
            noProxy: z.array(z.string()).optional(),
        }))
            .and(ExtensibleSchema));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.SocksProxyConfigurationSchema = z.lazy(() => z.object({
            socksProxy: z.string(),
            socksVersion: z.number().int().nonnegative().gte(0).lte(255),
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.PacProxyConfigurationSchema = z.lazy(() => z
            .object({
            proxyType: z.literal('pac'),
            proxyAutoconfigUrl: z.string(),
        })
            .and(ExtensibleSchema));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.SystemProxyConfigurationSchema = z.lazy(() => z
            .object({
            proxyType: z.literal('system'),
        })
            .and(ExtensibleSchema));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.UserPromptHandlerSchema = z.lazy(() => z.object({
            alert: Session.UserPromptHandlerTypeSchema.optional(),
            beforeUnload: Session.UserPromptHandlerTypeSchema.optional(),
            confirm: Session.UserPromptHandlerTypeSchema.optional(),
            default: Session.UserPromptHandlerTypeSchema.optional(),
            prompt: Session.UserPromptHandlerTypeSchema.optional(),
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.UserPromptHandlerTypeSchema = z.lazy(() => z.enum(['accept', 'dismiss', 'ignore']));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.SubscriptionRequestSchema = z.lazy(() => z.object({
            events: z.array(z.string()).min(1),
            contexts: z
                .array(BrowsingContext$1.BrowsingContextSchema)
                .min(1)
                .optional(),
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.StatusSchema = z.lazy(() => z.object({
            method: z.literal('session.status'),
            params: EmptyParamsSchema,
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.StatusResultSchema = z.lazy(() => z.object({
            ready: z.boolean(),
            message: z.string(),
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.NewSchema = z.lazy(() => z.object({
            method: z.literal('session.new'),
            params: Session.NewParametersSchema,
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.NewParametersSchema = z.lazy(() => z.object({
            capabilities: Session.CapabilitiesRequestSchema,
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.NewResultSchema = z.lazy(() => z.object({
            sessionId: z.string(),
            capabilities: z
                .object({
                acceptInsecureCerts: z.boolean(),
                browserName: z.string(),
                browserVersion: z.string(),
                platformName: z.string(),
                setWindowRect: z.boolean(),
                userAgent: z.string(),
                proxy: Session.ProxyConfigurationSchema.optional(),
                unhandledPromptBehavior: Session.UserPromptHandlerSchema.optional(),
                webSocketUrl: z.string().optional(),
            })
                .and(ExtensibleSchema),
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.EndSchema = z.lazy(() => z.object({
            method: z.literal('session.end'),
            params: EmptyParamsSchema,
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.SubscribeSchema = z.lazy(() => z.object({
            method: z.literal('session.subscribe'),
            params: Session.SubscriptionRequestSchema,
        }));
    })(Session$1 || (Session$1 = {}));
    (function (Session) {
        Session.UnsubscribeSchema = z.lazy(() => z.object({
            method: z.literal('session.unsubscribe'),
            params: Session.SubscriptionRequestSchema,
        }));
    })(Session$1 || (Session$1 = {}));
    const BrowserCommandSchema = z.lazy(() => z.union([
        Browser$1.CloseSchema,
        Browser$1.CreateUserContextSchema,
        Browser$1.GetClientWindowsSchema,
        Browser$1.GetUserContextsSchema,
        Browser$1.RemoveUserContextSchema,
        Browser$1.SetClientWindowStateSchema,
        z.object({}),
    ]));
    z.lazy(() => z.union([
        Browser$1.CreateUserContextResultSchema,
        Browser$1.GetUserContextsResultSchema,
    ]));
    var Browser$1;
    (function (Browser) {
        Browser.ClientWindowSchema = z.lazy(() => z.string());
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.ClientWindowInfoSchema = z.lazy(() => z.object({
            active: z.boolean(),
            clientWindow: Browser.ClientWindowSchema,
            height: JsUintSchema,
            state: z.enum(['fullscreen', 'maximized', 'minimized', 'normal']),
            width: JsUintSchema,
            x: JsIntSchema,
            y: JsIntSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.UserContextSchema = z.lazy(() => z.string());
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.UserContextInfoSchema = z.lazy(() => z.object({
            userContext: Browser.UserContextSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.CloseSchema = z.lazy(() => z.object({
            method: z.literal('browser.close'),
            params: EmptyParamsSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.CreateUserContextSchema = z.lazy(() => z.object({
            method: z.literal('browser.createUserContext'),
            params: EmptyParamsSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.CreateUserContextResultSchema = z.lazy(() => Browser.UserContextInfoSchema);
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.GetClientWindowsSchema = z.lazy(() => z.object({
            method: z.literal('browser.getClientWindows'),
            params: EmptyParamsSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.GetClientWindowsResultSchema = z.lazy(() => z.object({
            clientWindows: z.array(Browser.ClientWindowInfoSchema),
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.GetUserContextsSchema = z.lazy(() => z.object({
            method: z.literal('browser.getUserContexts'),
            params: EmptyParamsSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.GetUserContextsResultSchema = z.lazy(() => z.object({
            userContexts: z.array(Browser.UserContextInfoSchema).min(1),
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.RemoveUserContextSchema = z.lazy(() => z.object({
            method: z.literal('browser.removeUserContext'),
            params: Browser.RemoveUserContextParametersSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.RemoveUserContextParametersSchema = z.lazy(() => z.object({
            userContext: Browser.UserContextSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.SetClientWindowStateSchema = z.lazy(() => z.object({
            method: z.literal('browser.setClientWindowState'),
            params: Browser.SetClientWindowStateParametersSchema,
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.SetClientWindowStateParametersSchema = z.lazy(() => z.union([
            z
                .object({
                clientWindow: Browser.ClientWindowSchema,
            })
                .and(Browser.ClientWindowNamedStateSchema),
            Browser.ClientWindowRectStateSchema,
        ]));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.ClientWindowNamedStateSchema = z.lazy(() => z.object({
            state: z.enum(['fullscreen', 'maximized', 'minimized']),
        }));
    })(Browser$1 || (Browser$1 = {}));
    (function (Browser) {
        Browser.ClientWindowRectStateSchema = z.lazy(() => z.object({
            state: z.literal('normal'),
            width: JsUintSchema.optional(),
            height: JsUintSchema.optional(),
            x: JsIntSchema.optional(),
            y: JsIntSchema.optional(),
        }));
    })(Browser$1 || (Browser$1 = {}));
    const BrowsingContextCommandSchema = z.lazy(() => z.union([
        BrowsingContext$1.ActivateSchema,
        BrowsingContext$1.CaptureScreenshotSchema,
        BrowsingContext$1.CloseSchema,
        BrowsingContext$1.CreateSchema,
        BrowsingContext$1.GetTreeSchema,
        BrowsingContext$1.HandleUserPromptSchema,
        BrowsingContext$1.LocateNodesSchema,
        BrowsingContext$1.NavigateSchema,
        BrowsingContext$1.PrintSchema,
        BrowsingContext$1.ReloadSchema,
        BrowsingContext$1.SetViewportSchema,
        BrowsingContext$1.TraverseHistorySchema,
    ]));
    const BrowsingContextEventSchema = z.lazy(() => z.union([
        BrowsingContext$1.ContextCreatedSchema,
        BrowsingContext$1.ContextDestroyedSchema,
        BrowsingContext$1.DomContentLoadedSchema,
        BrowsingContext$1.DownloadWillBeginSchema,
        BrowsingContext$1.FragmentNavigatedSchema,
        BrowsingContext$1.LoadSchema,
        BrowsingContext$1.NavigationAbortedSchema,
        BrowsingContext$1.NavigationFailedSchema,
        BrowsingContext$1.NavigationStartedSchema,
        BrowsingContext$1.UserPromptClosedSchema,
        BrowsingContext$1.UserPromptOpenedSchema,
    ]));
    const BrowsingContextResultSchema = z.lazy(() => z.union([
        BrowsingContext$1.CaptureScreenshotResultSchema,
        BrowsingContext$1.CreateResultSchema,
        BrowsingContext$1.GetTreeResultSchema,
        BrowsingContext$1.LocateNodesResultSchema,
        BrowsingContext$1.NavigateResultSchema,
        BrowsingContext$1.PrintResultSchema,
        BrowsingContext$1.TraverseHistoryResultSchema,
    ]));
    var BrowsingContext$1;
    (function (BrowsingContext) {
        BrowsingContext.BrowsingContextSchema = z.lazy(() => z.string());
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.InfoListSchema = z.lazy(() => z.array(BrowsingContext.InfoSchema));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.InfoSchema = z.lazy(() => z.object({
            children: z.union([BrowsingContext.InfoListSchema, z.null()]),
            clientWindow: Browser$1.ClientWindowSchema,
            context: BrowsingContext.BrowsingContextSchema,
            originalOpener: z.union([
                BrowsingContext.BrowsingContextSchema,
                z.null(),
            ]),
            url: z.string(),
            userContext: Browser$1.UserContextSchema,
            parent: z
                .union([BrowsingContext.BrowsingContextSchema, z.null()])
                .optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.LocatorSchema = z.lazy(() => z.union([
            BrowsingContext.AccessibilityLocatorSchema,
            BrowsingContext.CssLocatorSchema,
            BrowsingContext.InnerTextLocatorSchema,
            BrowsingContext.XPathLocatorSchema,
        ]));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.AccessibilityLocatorSchema = z.lazy(() => z.object({
            type: z.literal('accessibility'),
            value: z.object({
                name: z.string().optional(),
                role: z.string().optional(),
            }),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CssLocatorSchema = z.lazy(() => z.object({
            type: z.literal('css'),
            value: z.string(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.InnerTextLocatorSchema = z.lazy(() => z.object({
            type: z.literal('innerText'),
            value: z.string(),
            ignoreCase: z.boolean().optional(),
            matchType: z.enum(['full', 'partial']).optional(),
            maxDepth: JsUintSchema.optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.XPathLocatorSchema = z.lazy(() => z.object({
            type: z.literal('xpath'),
            value: z.string(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigationSchema = z.lazy(() => z.string());
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigationInfoSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            navigation: z.union([BrowsingContext.NavigationSchema, z.null()]),
            timestamp: JsUintSchema,
            url: z.string(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ReadinessStateSchema = z.lazy(() => z.enum(['none', 'interactive', 'complete']));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.UserPromptTypeSchema = z.lazy(() => z.enum(['alert', 'beforeunload', 'confirm', 'prompt']));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ActivateSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.activate'),
            params: BrowsingContext.ActivateParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ActivateParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CaptureScreenshotParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            origin: z.enum(['viewport', 'document']).default('viewport').optional(),
            format: BrowsingContext.ImageFormatSchema.optional(),
            clip: BrowsingContext.ClipRectangleSchema.optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CaptureScreenshotSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.captureScreenshot'),
            params: BrowsingContext.CaptureScreenshotParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ImageFormatSchema = z.lazy(() => z.object({
            type: z.string(),
            quality: z.number().gte(0).lte(1).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ClipRectangleSchema = z.lazy(() => z.union([
            BrowsingContext.BoxClipRectangleSchema,
            BrowsingContext.ElementClipRectangleSchema,
        ]));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ElementClipRectangleSchema = z.lazy(() => z.object({
            type: z.literal('element'),
            element: Script$1.SharedReferenceSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.BoxClipRectangleSchema = z.lazy(() => z.object({
            type: z.literal('box'),
            x: z.number(),
            y: z.number(),
            width: z.number(),
            height: z.number(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CaptureScreenshotResultSchema = z.lazy(() => z.object({
            data: z.string(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CloseSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.close'),
            params: BrowsingContext.CloseParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CloseParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            promptUnload: z.boolean().default(false).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CreateSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.create'),
            params: BrowsingContext.CreateParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CreateTypeSchema = z.lazy(() => z.enum(['tab', 'window']));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CreateParametersSchema = z.lazy(() => z.object({
            type: BrowsingContext.CreateTypeSchema,
            referenceContext: BrowsingContext.BrowsingContextSchema.optional(),
            background: z.boolean().default(false).optional(),
            userContext: Browser$1.UserContextSchema.optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.CreateResultSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.GetTreeSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.getTree'),
            params: BrowsingContext.GetTreeParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.GetTreeParametersSchema = z.lazy(() => z.object({
            maxDepth: JsUintSchema.optional(),
            root: BrowsingContext.BrowsingContextSchema.optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.GetTreeResultSchema = z.lazy(() => z.object({
            contexts: BrowsingContext.InfoListSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.HandleUserPromptSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.handleUserPrompt'),
            params: BrowsingContext.HandleUserPromptParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.HandleUserPromptParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            accept: z.boolean().optional(),
            userText: z.string().optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.LocateNodesParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            locator: BrowsingContext.LocatorSchema,
            maxNodeCount: JsUintSchema.gte(1).optional(),
            serializationOptions: Script$1.SerializationOptionsSchema.optional(),
            startNodes: z.array(Script$1.SharedReferenceSchema).min(1).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.LocateNodesSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.locateNodes'),
            params: BrowsingContext.LocateNodesParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.LocateNodesResultSchema = z.lazy(() => z.object({
            nodes: z.array(Script$1.NodeRemoteValueSchema),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigateSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.navigate'),
            params: BrowsingContext.NavigateParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigateParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            url: z.string(),
            wait: BrowsingContext.ReadinessStateSchema.optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigateResultSchema = z.lazy(() => z.object({
            navigation: z.union([BrowsingContext.NavigationSchema, z.null()]),
            url: z.string(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.PrintSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.print'),
            params: BrowsingContext.PrintParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.PrintParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            background: z.boolean().default(false).optional(),
            margin: BrowsingContext.PrintMarginParametersSchema.optional(),
            orientation: z
                .enum(['portrait', 'landscape'])
                .default('portrait')
                .optional(),
            page: BrowsingContext.PrintPageParametersSchema.optional(),
            pageRanges: z.array(z.union([JsUintSchema, z.string()])).optional(),
            scale: z.number().gte(0.1).lte(2).default(1).optional(),
            shrinkToFit: z.boolean().default(true).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.PrintMarginParametersSchema = z.lazy(() => z.object({
            bottom: z.number().gte(0).default(1).optional(),
            left: z.number().gte(0).default(1).optional(),
            right: z.number().gte(0).default(1).optional(),
            top: z.number().gte(0).default(1).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.PrintPageParametersSchema = z.lazy(() => z.object({
            height: z.number().gte(0.0352).default(27.94).optional(),
            width: z.number().gte(0.0352).default(21.59).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.PrintResultSchema = z.lazy(() => z.object({
            data: z.string(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ReloadSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.reload'),
            params: BrowsingContext.ReloadParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ReloadParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            ignoreCache: z.boolean().optional(),
            wait: BrowsingContext.ReadinessStateSchema.optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.SetViewportSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.setViewport'),
            params: BrowsingContext.SetViewportParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.SetViewportParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            viewport: z.union([BrowsingContext.ViewportSchema, z.null()]).optional(),
            devicePixelRatio: z.union([z.number().gt(0), z.null()]).optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ViewportSchema = z.lazy(() => z.object({
            width: JsUintSchema,
            height: JsUintSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.TraverseHistorySchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.traverseHistory'),
            params: BrowsingContext.TraverseHistoryParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.TraverseHistoryParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            delta: JsIntSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.TraverseHistoryResultSchema = z.lazy(() => z.object({}));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ContextCreatedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.contextCreated'),
            params: BrowsingContext.InfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.ContextDestroyedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.contextDestroyed'),
            params: BrowsingContext.InfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigationStartedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.navigationStarted'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.FragmentNavigatedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.fragmentNavigated'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.DomContentLoadedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.domContentLoaded'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.LoadSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.load'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.DownloadWillBeginSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.downloadWillBegin'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigationAbortedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.navigationAborted'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.NavigationFailedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.navigationFailed'),
            params: BrowsingContext.NavigationInfoSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.UserPromptClosedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.userPromptClosed'),
            params: BrowsingContext.UserPromptClosedParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.UserPromptClosedParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            accepted: z.boolean(),
            type: BrowsingContext.UserPromptTypeSchema,
            userText: z.string().optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.UserPromptOpenedSchema = z.lazy(() => z.object({
            method: z.literal('browsingContext.userPromptOpened'),
            params: BrowsingContext.UserPromptOpenedParametersSchema,
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    (function (BrowsingContext) {
        BrowsingContext.UserPromptOpenedParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext.BrowsingContextSchema,
            handler: Session$1.UserPromptHandlerTypeSchema,
            message: z.string(),
            type: BrowsingContext.UserPromptTypeSchema,
            defaultValue: z.string().optional(),
        }));
    })(BrowsingContext$1 || (BrowsingContext$1 = {}));
    const NetworkCommandSchema = z.lazy(() => z.union([
        Network$1.AddInterceptSchema,
        Network$1.ContinueRequestSchema,
        Network$1.ContinueResponseSchema,
        Network$1.ContinueWithAuthSchema,
        Network$1.FailRequestSchema,
        Network$1.ProvideResponseSchema,
        Network$1.RemoveInterceptSchema,
        Network$1.SetCacheBehaviorSchema,
    ]));
    const NetworkEventSchema = z.lazy(() => z.union([
        Network$1.AuthRequiredSchema,
        Network$1.BeforeRequestSentSchema,
        Network$1.FetchErrorSchema,
        Network$1.ResponseCompletedSchema,
        Network$1.ResponseStartedSchema,
    ]));
    const NetworkResultSchema = z.lazy(() => Network$1.AddInterceptResultSchema);
    var Network$1;
    (function (Network) {
        Network.AuthChallengeSchema = z.lazy(() => z.object({
            scheme: z.string(),
            realm: z.string(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.AuthCredentialsSchema = z.lazy(() => z.object({
            type: z.literal('password'),
            username: z.string(),
            password: z.string(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.BaseParametersSchema = z.lazy(() => z.object({
            context: z.union([BrowsingContext$1.BrowsingContextSchema, z.null()]),
            isBlocked: z.boolean(),
            navigation: z.union([BrowsingContext$1.NavigationSchema, z.null()]),
            redirectCount: JsUintSchema,
            request: Network.RequestDataSchema,
            timestamp: JsUintSchema,
            intercepts: z.array(Network.InterceptSchema).min(1).optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.BytesValueSchema = z.lazy(() => z.union([Network.StringValueSchema, Network.Base64ValueSchema]));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.StringValueSchema = z.lazy(() => z.object({
            type: z.literal('string'),
            value: z.string(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.Base64ValueSchema = z.lazy(() => z.object({
            type: z.literal('base64'),
            value: z.string(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.SameSiteSchema = z.lazy(() => z.enum(['strict', 'lax', 'none']));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.CookieSchema = z.lazy(() => z
            .object({
            name: z.string(),
            value: Network.BytesValueSchema,
            domain: z.string(),
            path: z.string(),
            size: JsUintSchema,
            httpOnly: z.boolean(),
            secure: z.boolean(),
            sameSite: Network.SameSiteSchema,
            expiry: JsUintSchema.optional(),
        })
            .and(ExtensibleSchema));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.CookieHeaderSchema = z.lazy(() => z.object({
            name: z.string(),
            value: Network.BytesValueSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.FetchTimingInfoSchema = z.lazy(() => z.object({
            timeOrigin: z.number(),
            requestTime: z.number(),
            redirectStart: z.number(),
            redirectEnd: z.number(),
            fetchStart: z.number(),
            dnsStart: z.number(),
            dnsEnd: z.number(),
            connectStart: z.number(),
            connectEnd: z.number(),
            tlsStart: z.number(),
            requestStart: z.number(),
            responseStart: z.number(),
            responseEnd: z.number(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.HeaderSchema = z.lazy(() => z.object({
            name: z.string(),
            value: Network.BytesValueSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.InitiatorSchema = z.lazy(() => z.object({
            type: z.enum(['parser', 'script', 'preflight', 'other']),
            columnNumber: JsUintSchema.optional(),
            lineNumber: JsUintSchema.optional(),
            stackTrace: Script$1.StackTraceSchema.optional(),
            request: Network.RequestSchema.optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.InterceptSchema = z.lazy(() => z.string());
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.RequestSchema = z.lazy(() => z.string());
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.RequestDataSchema = z.lazy(() => z.object({
            request: Network.RequestSchema,
            url: z.string(),
            method: z.string(),
            headers: z.array(Network.HeaderSchema),
            cookies: z.array(Network.CookieSchema),
            headersSize: JsUintSchema,
            bodySize: z.union([JsUintSchema, z.null()]),
            timings: Network.FetchTimingInfoSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ResponseContentSchema = z.lazy(() => z.object({
            size: JsUintSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ResponseDataSchema = z.lazy(() => z.object({
            url: z.string(),
            protocol: z.string(),
            status: JsUintSchema,
            statusText: z.string(),
            fromCache: z.boolean(),
            headers: z.array(Network.HeaderSchema),
            mimeType: z.string(),
            bytesReceived: JsUintSchema,
            headersSize: z.union([JsUintSchema, z.null()]),
            bodySize: z.union([JsUintSchema, z.null()]),
            content: Network.ResponseContentSchema,
            authChallenges: z.array(Network.AuthChallengeSchema).optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.SetCookieHeaderSchema = z.lazy(() => z.object({
            name: z.string(),
            value: Network.BytesValueSchema,
            domain: z.string().optional(),
            httpOnly: z.boolean().optional(),
            expiry: z.string().optional(),
            maxAge: JsIntSchema.optional(),
            path: z.string().optional(),
            sameSite: Network.SameSiteSchema.optional(),
            secure: z.boolean().optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.UrlPatternSchema = z.lazy(() => z.union([Network.UrlPatternPatternSchema, Network.UrlPatternStringSchema]));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.UrlPatternPatternSchema = z.lazy(() => z.object({
            type: z.literal('pattern'),
            protocol: z.string().optional(),
            hostname: z.string().optional(),
            port: z.string().optional(),
            pathname: z.string().optional(),
            search: z.string().optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.UrlPatternStringSchema = z.lazy(() => z.object({
            type: z.literal('string'),
            pattern: z.string(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.AddInterceptParametersSchema = z.lazy(() => z.object({
            phases: z.array(Network.InterceptPhaseSchema).min(1),
            contexts: z
                .array(BrowsingContext$1.BrowsingContextSchema)
                .min(1)
                .optional(),
            urlPatterns: z.array(Network.UrlPatternSchema).optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.AddInterceptSchema = z.lazy(() => z.object({
            method: z.literal('network.addIntercept'),
            params: Network.AddInterceptParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.InterceptPhaseSchema = z.lazy(() => z.enum(['beforeRequestSent', 'responseStarted', 'authRequired']));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.AddInterceptResultSchema = z.lazy(() => z.object({
            intercept: Network.InterceptSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueRequestSchema = z.lazy(() => z.object({
            method: z.literal('network.continueRequest'),
            params: Network.ContinueRequestParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueRequestParametersSchema = z.lazy(() => z.object({
            request: Network.RequestSchema,
            body: Network.BytesValueSchema.optional(),
            cookies: z.array(Network.CookieHeaderSchema).optional(),
            headers: z.array(Network.HeaderSchema).optional(),
            method: z.string().optional(),
            url: z.string().optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueResponseSchema = z.lazy(() => z.object({
            method: z.literal('network.continueResponse'),
            params: Network.ContinueResponseParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueResponseParametersSchema = z.lazy(() => z.object({
            request: Network.RequestSchema,
            cookies: z.array(Network.SetCookieHeaderSchema).optional(),
            credentials: Network.AuthCredentialsSchema.optional(),
            headers: z.array(Network.HeaderSchema).optional(),
            reasonPhrase: z.string().optional(),
            statusCode: JsUintSchema.optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueWithAuthSchema = z.lazy(() => z.object({
            method: z.literal('network.continueWithAuth'),
            params: Network.ContinueWithAuthParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueWithAuthParametersSchema = z.lazy(() => z
            .object({
            request: Network.RequestSchema,
        })
            .and(z.union([
            Network.ContinueWithAuthCredentialsSchema,
            Network.ContinueWithAuthNoCredentialsSchema,
        ])));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueWithAuthCredentialsSchema = z.lazy(() => z.object({
            action: z.literal('provideCredentials'),
            credentials: Network.AuthCredentialsSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ContinueWithAuthNoCredentialsSchema = z.lazy(() => z.object({
            action: z.enum(['default', 'cancel']),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.FailRequestSchema = z.lazy(() => z.object({
            method: z.literal('network.failRequest'),
            params: Network.FailRequestParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.FailRequestParametersSchema = z.lazy(() => z.object({
            request: Network.RequestSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ProvideResponseSchema = z.lazy(() => z.object({
            method: z.literal('network.provideResponse'),
            params: Network.ProvideResponseParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ProvideResponseParametersSchema = z.lazy(() => z.object({
            request: Network.RequestSchema,
            body: Network.BytesValueSchema.optional(),
            cookies: z.array(Network.SetCookieHeaderSchema).optional(),
            headers: z.array(Network.HeaderSchema).optional(),
            reasonPhrase: z.string().optional(),
            statusCode: JsUintSchema.optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.RemoveInterceptSchema = z.lazy(() => z.object({
            method: z.literal('network.removeIntercept'),
            params: Network.RemoveInterceptParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.RemoveInterceptParametersSchema = z.lazy(() => z.object({
            intercept: Network.InterceptSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.SetCacheBehaviorSchema = z.lazy(() => z.object({
            method: z.literal('network.setCacheBehavior'),
            params: Network.SetCacheBehaviorParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.SetCacheBehaviorParametersSchema = z.lazy(() => z.object({
            cacheBehavior: z.enum(['default', 'bypass']),
            contexts: z
                .array(BrowsingContext$1.BrowsingContextSchema)
                .min(1)
                .optional(),
        }));
    })(Network$1 || (Network$1 = {}));
    const ScriptEventSchema = z.lazy(() => z.union([
        Script$1.MessageSchema,
        Script$1.RealmCreatedSchema,
        Script$1.RealmDestroyedSchema,
    ]));
    (function (Network) {
        Network.AuthRequiredParametersSchema = z.lazy(() => Network.BaseParametersSchema.and(z.object({
            response: Network.ResponseDataSchema,
        })));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.BeforeRequestSentParametersSchema = z.lazy(() => Network.BaseParametersSchema.and(z.object({
            initiator: Network.InitiatorSchema,
        })));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.FetchErrorParametersSchema = z.lazy(() => Network.BaseParametersSchema.and(z.object({
            errorText: z.string(),
        })));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ResponseCompletedParametersSchema = z.lazy(() => Network.BaseParametersSchema.and(z.object({
            response: Network.ResponseDataSchema,
        })));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ResponseStartedParametersSchema = z.lazy(() => Network.BaseParametersSchema.and(z.object({
            response: Network.ResponseDataSchema,
        })));
    })(Network$1 || (Network$1 = {}));
    const ScriptCommandSchema = z.lazy(() => z.union([
        Script$1.AddPreloadScriptSchema,
        Script$1.CallFunctionSchema,
        Script$1.DisownSchema,
        Script$1.EvaluateSchema,
        Script$1.GetRealmsSchema,
        Script$1.RemovePreloadScriptSchema,
    ]));
    const ScriptResultSchema = z.lazy(() => z.union([
        Script$1.AddPreloadScriptResultSchema,
        Script$1.EvaluateResultSchema,
        Script$1.GetRealmsResultSchema,
    ]));
    (function (Network) {
        Network.AuthRequiredSchema = z.lazy(() => z.object({
            method: z.literal('network.authRequired'),
            params: Network.AuthRequiredParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.BeforeRequestSentSchema = z.lazy(() => z.object({
            method: z.literal('network.beforeRequestSent'),
            params: Network.BeforeRequestSentParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.FetchErrorSchema = z.lazy(() => z.object({
            method: z.literal('network.fetchError'),
            params: Network.FetchErrorParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ResponseCompletedSchema = z.lazy(() => z.object({
            method: z.literal('network.responseCompleted'),
            params: Network.ResponseCompletedParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    (function (Network) {
        Network.ResponseStartedSchema = z.lazy(() => z.object({
            method: z.literal('network.responseStarted'),
            params: Network.ResponseStartedParametersSchema,
        }));
    })(Network$1 || (Network$1 = {}));
    var Script$1;
    (function (Script) {
        Script.ChannelSchema = z.lazy(() => z.string());
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.EvaluateResultSuccessSchema = z.lazy(() => z.object({
            type: z.literal('success'),
            result: Script.RemoteValueSchema,
            realm: Script.RealmSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ExceptionDetailsSchema = z.lazy(() => z.object({
            columnNumber: JsUintSchema,
            exception: Script.RemoteValueSchema,
            lineNumber: JsUintSchema,
            stackTrace: Script.StackTraceSchema,
            text: z.string(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ChannelValueSchema = z.lazy(() => z.object({
            type: z.literal('channel'),
            value: Script.ChannelPropertiesSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ChannelPropertiesSchema = z.lazy(() => z.object({
            channel: Script.ChannelSchema,
            serializationOptions: Script.SerializationOptionsSchema.optional(),
            ownership: Script.ResultOwnershipSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.EvaluateResultSchema = z.lazy(() => z.union([
            Script.EvaluateResultSuccessSchema,
            Script.EvaluateResultExceptionSchema,
        ]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.EvaluateResultExceptionSchema = z.lazy(() => z.object({
            type: z.literal('exception'),
            exceptionDetails: Script.ExceptionDetailsSchema,
            realm: Script.RealmSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.HandleSchema = z.lazy(() => z.string());
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.InternalIdSchema = z.lazy(() => z.string());
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ListLocalValueSchema = z.lazy(() => z.array(Script.LocalValueSchema));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.LocalValueSchema = z.lazy(() => z.union([
            Script.RemoteReferenceSchema,
            Script.PrimitiveProtocolValueSchema,
            Script.ChannelValueSchema,
            Script.ArrayLocalValueSchema,
            Script.DateLocalValueSchema,
            Script.MapLocalValueSchema,
            Script.ObjectLocalValueSchema,
            Script.RegExpLocalValueSchema,
            Script.SetLocalValueSchema,
        ]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ArrayLocalValueSchema = z.lazy(() => z.object({
            type: z.literal('array'),
            value: Script.ListLocalValueSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.DateLocalValueSchema = z.lazy(() => z.object({
            type: z.literal('date'),
            value: z.string(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.MappingLocalValueSchema = z.lazy(() => z.array(z.tuple([
            z.union([Script.LocalValueSchema, z.string()]),
            Script.LocalValueSchema,
        ])));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.MapLocalValueSchema = z.lazy(() => z.object({
            type: z.literal('map'),
            value: Script.MappingLocalValueSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ObjectLocalValueSchema = z.lazy(() => z.object({
            type: z.literal('object'),
            value: Script.MappingLocalValueSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RegExpValueSchema = z.lazy(() => z.object({
            pattern: z.string(),
            flags: z.string().optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RegExpLocalValueSchema = z.lazy(() => z.object({
            type: z.literal('regexp'),
            value: Script.RegExpValueSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SetLocalValueSchema = z.lazy(() => z.object({
            type: z.literal('set'),
            value: Script.ListLocalValueSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.PreloadScriptSchema = z.lazy(() => z.string());
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmSchema = z.lazy(() => z.string());
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.PrimitiveProtocolValueSchema = z.lazy(() => z.union([
            Script.UndefinedValueSchema,
            Script.NullValueSchema,
            Script.StringValueSchema,
            Script.NumberValueSchema,
            Script.BooleanValueSchema,
            Script.BigIntValueSchema,
        ]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.UndefinedValueSchema = z.lazy(() => z.object({
            type: z.literal('undefined'),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.NullValueSchema = z.lazy(() => z.object({
            type: z.literal('null'),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.StringValueSchema = z.lazy(() => z.object({
            type: z.literal('string'),
            value: z.string(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SpecialNumberSchema = z.lazy(() => z.enum(['NaN', '-0', 'Infinity', '-Infinity']));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.NumberValueSchema = z.lazy(() => z.object({
            type: z.literal('number'),
            value: z.union([z.number(), Script.SpecialNumberSchema]),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.BooleanValueSchema = z.lazy(() => z.object({
            type: z.literal('boolean'),
            value: z.boolean(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.BigIntValueSchema = z.lazy(() => z.object({
            type: z.literal('bigint'),
            value: z.string(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmInfoSchema = z.lazy(() => z.union([
            Script.WindowRealmInfoSchema,
            Script.DedicatedWorkerRealmInfoSchema,
            Script.SharedWorkerRealmInfoSchema,
            Script.ServiceWorkerRealmInfoSchema,
            Script.WorkerRealmInfoSchema,
            Script.PaintWorkletRealmInfoSchema,
            Script.AudioWorkletRealmInfoSchema,
            Script.WorkletRealmInfoSchema,
        ]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.BaseRealmInfoSchema = z.lazy(() => z.object({
            realm: Script.RealmSchema,
            origin: z.string(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WindowRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('window'),
            context: BrowsingContext$1.BrowsingContextSchema,
            sandbox: z.string().optional(),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.DedicatedWorkerRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('dedicated-worker'),
            owners: z.tuple([Script.RealmSchema]),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SharedWorkerRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('shared-worker'),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ServiceWorkerRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('service-worker'),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WorkerRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('worker'),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.PaintWorkletRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('paint-worklet'),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.AudioWorkletRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('audio-worklet'),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WorkletRealmInfoSchema = z.lazy(() => Script.BaseRealmInfoSchema.and(z.object({
            type: z.literal('worklet'),
        })));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmTypeSchema = z.lazy(() => z.enum([
            'window',
            'dedicated-worker',
            'shared-worker',
            'service-worker',
            'worker',
            'paint-worklet',
            'audio-worklet',
            'worklet',
        ]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ListRemoteValueSchema = z.lazy(() => z.array(Script.RemoteValueSchema));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.MappingRemoteValueSchema = z.lazy(() => z.array(z.tuple([
            z.union([Script.RemoteValueSchema, z.string()]),
            Script.RemoteValueSchema,
        ])));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RemoteValueSchema = z.lazy(() => z.union([
            Script.PrimitiveProtocolValueSchema,
            Script.SymbolRemoteValueSchema,
            Script.ArrayRemoteValueSchema,
            Script.ObjectRemoteValueSchema,
            Script.FunctionRemoteValueSchema,
            Script.RegExpRemoteValueSchema,
            Script.DateRemoteValueSchema,
            Script.MapRemoteValueSchema,
            Script.SetRemoteValueSchema,
            Script.WeakMapRemoteValueSchema,
            Script.WeakSetRemoteValueSchema,
            Script.GeneratorRemoteValueSchema,
            Script.ErrorRemoteValueSchema,
            Script.ProxyRemoteValueSchema,
            Script.PromiseRemoteValueSchema,
            Script.TypedArrayRemoteValueSchema,
            Script.ArrayBufferRemoteValueSchema,
            Script.NodeListRemoteValueSchema,
            Script.HtmlCollectionRemoteValueSchema,
            Script.NodeRemoteValueSchema,
            Script.WindowProxyRemoteValueSchema,
        ]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RemoteReferenceSchema = z.lazy(() => z.union([Script.SharedReferenceSchema, Script.RemoteObjectReferenceSchema]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SharedReferenceSchema = z.lazy(() => z
            .object({
            sharedId: Script.SharedIdSchema,
            handle: Script.HandleSchema.optional(),
        })
            .and(ExtensibleSchema));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RemoteObjectReferenceSchema = z.lazy(() => z
            .object({
            handle: Script.HandleSchema,
            sharedId: Script.SharedIdSchema.optional(),
        })
            .and(ExtensibleSchema));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SymbolRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('symbol'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ArrayRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('array'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.ListRemoteValueSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ObjectRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('object'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.MappingRemoteValueSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.FunctionRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('function'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RegExpRemoteValueSchema = z.lazy(() => z
            .object({
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        })
            .and(Script.RegExpLocalValueSchema));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.DateRemoteValueSchema = z.lazy(() => z
            .object({
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        })
            .and(Script.DateLocalValueSchema));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.MapRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('map'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.MappingRemoteValueSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SetRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('set'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.ListRemoteValueSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WeakMapRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('weakmap'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WeakSetRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('weakset'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.GeneratorRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('generator'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ErrorRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('error'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ProxyRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('proxy'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.PromiseRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('promise'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.TypedArrayRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('typedarray'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ArrayBufferRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('arraybuffer'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.NodeListRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('nodelist'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.ListRemoteValueSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.HtmlCollectionRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('htmlcollection'),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.ListRemoteValueSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.NodeRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('node'),
            sharedId: Script.SharedIdSchema.optional(),
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
            value: Script.NodePropertiesSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.NodePropertiesSchema = z.lazy(() => z.object({
            nodeType: JsUintSchema,
            childNodeCount: JsUintSchema,
            attributes: z.record(z.string(), z.string()).optional(),
            children: z.array(Script.NodeRemoteValueSchema).optional(),
            localName: z.string().optional(),
            mode: z.enum(['open', 'closed']).optional(),
            namespaceURI: z.string().optional(),
            nodeValue: z.string().optional(),
            shadowRoot: z.union([Script.NodeRemoteValueSchema, z.null()]).optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WindowProxyRemoteValueSchema = z.lazy(() => z.object({
            type: z.literal('window'),
            value: Script.WindowProxyPropertiesSchema,
            handle: Script.HandleSchema.optional(),
            internalId: Script.InternalIdSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.WindowProxyPropertiesSchema = z.lazy(() => z.object({
            context: BrowsingContext$1.BrowsingContextSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ResultOwnershipSchema = z.lazy(() => z.enum(['root', 'none']));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SerializationOptionsSchema = z.lazy(() => z.object({
            maxDomDepth: z.union([JsUintSchema, z.null()]).default(0).optional(),
            maxObjectDepth: z
                .union([JsUintSchema, z.null()])
                .default(null)
                .optional(),
            includeShadowTree: z
                .enum(['none', 'open', 'all'])
                .default('none')
                .optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SharedIdSchema = z.lazy(() => z.string());
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.StackFrameSchema = z.lazy(() => z.object({
            columnNumber: JsUintSchema,
            functionName: z.string(),
            lineNumber: JsUintSchema,
            url: z.string(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.StackTraceSchema = z.lazy(() => z.object({
            callFrames: z.array(Script.StackFrameSchema),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.SourceSchema = z.lazy(() => z.object({
            realm: Script.RealmSchema,
            context: BrowsingContext$1.BrowsingContextSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmTargetSchema = z.lazy(() => z.object({
            realm: Script.RealmSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.ContextTargetSchema = z.lazy(() => z.object({
            context: BrowsingContext$1.BrowsingContextSchema,
            sandbox: z.string().optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.TargetSchema = z.lazy(() => z.union([Script.ContextTargetSchema, Script.RealmTargetSchema]));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.AddPreloadScriptSchema = z.lazy(() => z.object({
            method: z.literal('script.addPreloadScript'),
            params: Script.AddPreloadScriptParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.AddPreloadScriptParametersSchema = z.lazy(() => z.object({
            functionDeclaration: z.string(),
            arguments: z.array(Script.ChannelValueSchema).optional(),
            contexts: z
                .array(BrowsingContext$1.BrowsingContextSchema)
                .min(1)
                .optional(),
            sandbox: z.string().optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.AddPreloadScriptResultSchema = z.lazy(() => z.object({
            script: Script.PreloadScriptSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.DisownSchema = z.lazy(() => z.object({
            method: z.literal('script.disown'),
            params: Script.DisownParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.DisownParametersSchema = z.lazy(() => z.object({
            handles: z.array(Script.HandleSchema),
            target: Script.TargetSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.CallFunctionParametersSchema = z.lazy(() => z.object({
            functionDeclaration: z.string(),
            awaitPromise: z.boolean(),
            target: Script.TargetSchema,
            arguments: z.array(Script.LocalValueSchema).optional(),
            resultOwnership: Script.ResultOwnershipSchema.optional(),
            serializationOptions: Script.SerializationOptionsSchema.optional(),
            this: Script.LocalValueSchema.optional(),
            userActivation: z.boolean().default(false).optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.CallFunctionSchema = z.lazy(() => z.object({
            method: z.literal('script.callFunction'),
            params: Script.CallFunctionParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.EvaluateSchema = z.lazy(() => z.object({
            method: z.literal('script.evaluate'),
            params: Script.EvaluateParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.EvaluateParametersSchema = z.lazy(() => z.object({
            expression: z.string(),
            target: Script.TargetSchema,
            awaitPromise: z.boolean(),
            resultOwnership: Script.ResultOwnershipSchema.optional(),
            serializationOptions: Script.SerializationOptionsSchema.optional(),
            userActivation: z.boolean().default(false).optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.GetRealmsSchema = z.lazy(() => z.object({
            method: z.literal('script.getRealms'),
            params: Script.GetRealmsParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.GetRealmsParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext$1.BrowsingContextSchema.optional(),
            type: Script.RealmTypeSchema.optional(),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.GetRealmsResultSchema = z.lazy(() => z.object({
            realms: z.array(Script.RealmInfoSchema),
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RemovePreloadScriptSchema = z.lazy(() => z.object({
            method: z.literal('script.removePreloadScript'),
            params: Script.RemovePreloadScriptParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RemovePreloadScriptParametersSchema = z.lazy(() => z.object({
            script: Script.PreloadScriptSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.MessageParametersSchema = z.lazy(() => z.object({
            channel: Script.ChannelSchema,
            data: Script.RemoteValueSchema,
            source: Script.SourceSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmCreatedSchema = z.lazy(() => z.object({
            method: z.literal('script.realmCreated'),
            params: Script.RealmInfoSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.MessageSchema = z.lazy(() => z.object({
            method: z.literal('script.message'),
            params: Script.MessageParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmDestroyedSchema = z.lazy(() => z.object({
            method: z.literal('script.realmDestroyed'),
            params: Script.RealmDestroyedParametersSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    (function (Script) {
        Script.RealmDestroyedParametersSchema = z.lazy(() => z.object({
            realm: Script.RealmSchema,
        }));
    })(Script$1 || (Script$1 = {}));
    const StorageCommandSchema = z.lazy(() => z.union([
        Storage$1.DeleteCookiesSchema,
        Storage$1.GetCookiesSchema,
        Storage$1.SetCookieSchema,
    ]));
    const StorageResultSchema = z.lazy(() => z.union([
        Storage$1.DeleteCookiesResultSchema,
        Storage$1.GetCookiesResultSchema,
        Storage$1.SetCookieResultSchema,
    ]));
    var Storage$1;
    (function (Storage) {
        Storage.PartitionKeySchema = z.lazy(() => z
            .object({
            userContext: z.string().optional(),
            sourceOrigin: z.string().optional(),
        })
            .and(ExtensibleSchema));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.GetCookiesSchema = z.lazy(() => z.object({
            method: z.literal('storage.getCookies'),
            params: Storage.GetCookiesParametersSchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.CookieFilterSchema = z.lazy(() => z
            .object({
            name: z.string().optional(),
            value: Network$1.BytesValueSchema.optional(),
            domain: z.string().optional(),
            path: z.string().optional(),
            size: JsUintSchema.optional(),
            httpOnly: z.boolean().optional(),
            secure: z.boolean().optional(),
            sameSite: Network$1.SameSiteSchema.optional(),
            expiry: JsUintSchema.optional(),
        })
            .and(ExtensibleSchema));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.BrowsingContextPartitionDescriptorSchema = z.lazy(() => z.object({
            type: z.literal('context'),
            context: BrowsingContext$1.BrowsingContextSchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.StorageKeyPartitionDescriptorSchema = z.lazy(() => z
            .object({
            type: z.literal('storageKey'),
            userContext: z.string().optional(),
            sourceOrigin: z.string().optional(),
        })
            .and(ExtensibleSchema));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.PartitionDescriptorSchema = z.lazy(() => z.union([
            Storage.BrowsingContextPartitionDescriptorSchema,
            Storage.StorageKeyPartitionDescriptorSchema,
        ]));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.GetCookiesParametersSchema = z.lazy(() => z.object({
            filter: Storage.CookieFilterSchema.optional(),
            partition: Storage.PartitionDescriptorSchema.optional(),
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.GetCookiesResultSchema = z.lazy(() => z.object({
            cookies: z.array(Network$1.CookieSchema),
            partitionKey: Storage.PartitionKeySchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.SetCookieSchema = z.lazy(() => z.object({
            method: z.literal('storage.setCookie'),
            params: Storage.SetCookieParametersSchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.PartialCookieSchema = z.lazy(() => z
            .object({
            name: z.string(),
            value: Network$1.BytesValueSchema,
            domain: z.string(),
            path: z.string().optional(),
            httpOnly: z.boolean().optional(),
            secure: z.boolean().optional(),
            sameSite: Network$1.SameSiteSchema.optional(),
            expiry: JsUintSchema.optional(),
        })
            .and(ExtensibleSchema));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.SetCookieParametersSchema = z.lazy(() => z.object({
            cookie: Storage.PartialCookieSchema,
            partition: Storage.PartitionDescriptorSchema.optional(),
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.SetCookieResultSchema = z.lazy(() => z.object({
            partitionKey: Storage.PartitionKeySchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.DeleteCookiesSchema = z.lazy(() => z.object({
            method: z.literal('storage.deleteCookies'),
            params: Storage.DeleteCookiesParametersSchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.DeleteCookiesParametersSchema = z.lazy(() => z.object({
            filter: Storage.CookieFilterSchema.optional(),
            partition: Storage.PartitionDescriptorSchema.optional(),
        }));
    })(Storage$1 || (Storage$1 = {}));
    (function (Storage) {
        Storage.DeleteCookiesResultSchema = z.lazy(() => z.object({
            partitionKey: Storage.PartitionKeySchema,
        }));
    })(Storage$1 || (Storage$1 = {}));
    const LogEventSchema = z.lazy(() => Log.EntryAddedSchema);
    var Log;
    (function (Log) {
        Log.LevelSchema = z.lazy(() => z.enum(['debug', 'info', 'warn', 'error']));
    })(Log || (Log = {}));
    (function (Log) {
        Log.EntrySchema = z.lazy(() => z.union([
            Log.GenericLogEntrySchema,
            Log.ConsoleLogEntrySchema,
            Log.JavascriptLogEntrySchema,
        ]));
    })(Log || (Log = {}));
    (function (Log) {
        Log.BaseLogEntrySchema = z.lazy(() => z.object({
            level: Log.LevelSchema,
            source: Script$1.SourceSchema,
            text: z.union([z.string(), z.null()]),
            timestamp: JsUintSchema,
            stackTrace: Script$1.StackTraceSchema.optional(),
        }));
    })(Log || (Log = {}));
    (function (Log) {
        Log.GenericLogEntrySchema = z.lazy(() => Log.BaseLogEntrySchema.and(z.object({
            type: z.string(),
        })));
    })(Log || (Log = {}));
    (function (Log) {
        Log.ConsoleLogEntrySchema = z.lazy(() => Log.BaseLogEntrySchema.and(z.object({
            type: z.literal('console'),
            method: z.string(),
            args: z.array(Script$1.RemoteValueSchema),
        })));
    })(Log || (Log = {}));
    (function (Log) {
        Log.JavascriptLogEntrySchema = z.lazy(() => Log.BaseLogEntrySchema.and(z.object({
            type: z.literal('javascript'),
        })));
    })(Log || (Log = {}));
    (function (Log) {
        Log.EntryAddedSchema = z.lazy(() => z.object({
            method: z.literal('log.entryAdded'),
            params: Log.EntrySchema,
        }));
    })(Log || (Log = {}));
    const InputCommandSchema = z.lazy(() => z.union([
        Input$1.PerformActionsSchema,
        Input$1.ReleaseActionsSchema,
        Input$1.SetFilesSchema,
    ]));
    var Input$1;
    (function (Input) {
        Input.ElementOriginSchema = z.lazy(() => z.object({
            type: z.literal('element'),
            element: Script$1.SharedReferenceSchema,
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PerformActionsParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext$1.BrowsingContextSchema,
            actions: z.array(Input.SourceActionsSchema),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.NoneSourceActionsSchema = z.lazy(() => z.object({
            type: z.literal('none'),
            id: z.string(),
            actions: z.array(Input.NoneSourceActionSchema),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.KeySourceActionsSchema = z.lazy(() => z.object({
            type: z.literal('key'),
            id: z.string(),
            actions: z.array(Input.KeySourceActionSchema),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerSourceActionsSchema = z.lazy(() => z.object({
            type: z.literal('pointer'),
            id: z.string(),
            parameters: Input.PointerParametersSchema.optional(),
            actions: z.array(Input.PointerSourceActionSchema),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PerformActionsSchema = z.lazy(() => z.object({
            method: z.literal('input.performActions'),
            params: Input.PerformActionsParametersSchema,
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.SourceActionsSchema = z.lazy(() => z.union([
            Input.NoneSourceActionsSchema,
            Input.KeySourceActionsSchema,
            Input.PointerSourceActionsSchema,
            Input.WheelSourceActionsSchema,
        ]));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.NoneSourceActionSchema = z.lazy(() => Input.PauseActionSchema);
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.KeySourceActionSchema = z.lazy(() => z.union([
            Input.PauseActionSchema,
            Input.KeyDownActionSchema,
            Input.KeyUpActionSchema,
        ]));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerTypeSchema = z.lazy(() => z.enum(['mouse', 'pen', 'touch']));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerParametersSchema = z.lazy(() => z.object({
            pointerType: Input.PointerTypeSchema.default('mouse').optional(),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.WheelSourceActionsSchema = z.lazy(() => z.object({
            type: z.literal('wheel'),
            id: z.string(),
            actions: z.array(Input.WheelSourceActionSchema),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerSourceActionSchema = z.lazy(() => z.union([
            Input.PauseActionSchema,
            Input.PointerDownActionSchema,
            Input.PointerUpActionSchema,
            Input.PointerMoveActionSchema,
        ]));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.WheelSourceActionSchema = z.lazy(() => z.union([Input.PauseActionSchema, Input.WheelScrollActionSchema]));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PauseActionSchema = z.lazy(() => z.object({
            type: z.literal('pause'),
            duration: JsUintSchema.optional(),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.KeyDownActionSchema = z.lazy(() => z.object({
            type: z.literal('keyDown'),
            value: z.string(),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.KeyUpActionSchema = z.lazy(() => z.object({
            type: z.literal('keyUp'),
            value: z.string(),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerUpActionSchema = z.lazy(() => z.object({
            type: z.literal('pointerUp'),
            button: JsUintSchema,
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerDownActionSchema = z.lazy(() => z
            .object({
            type: z.literal('pointerDown'),
            button: JsUintSchema,
        })
            .and(Input.PointerCommonPropertiesSchema));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerMoveActionSchema = z.lazy(() => z
            .object({
            type: z.literal('pointerMove'),
            x: JsIntSchema,
            y: JsIntSchema,
            duration: JsUintSchema.optional(),
            origin: Input.OriginSchema.optional(),
        })
            .and(Input.PointerCommonPropertiesSchema));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.WheelScrollActionSchema = z.lazy(() => z.object({
            type: z.literal('scroll'),
            x: JsIntSchema,
            y: JsIntSchema,
            deltaX: JsIntSchema,
            deltaY: JsIntSchema,
            duration: JsUintSchema.optional(),
            origin: Input.OriginSchema.default('viewport').optional(),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.PointerCommonPropertiesSchema = z.lazy(() => z.object({
            width: JsUintSchema.default(1).optional(),
            height: JsUintSchema.default(1).optional(),
            pressure: z.number().default(0).optional(),
            tangentialPressure: z.number().default(0).optional(),
            twist: z
                .number()
                .int()
                .nonnegative()
                .gte(0)
                .lte(359)
                .default(0)
                .optional(),
            altitudeAngle: z
                .number()
                .gte(0)
                .lte(1.5707963267948966)
                .default(0)
                .optional(),
            azimuthAngle: z
                .number()
                .gte(0)
                .lte(6.283185307179586)
                .default(0)
                .optional(),
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.OriginSchema = z.lazy(() => z.union([
            z.literal('viewport'),
            z.literal('pointer'),
            Input.ElementOriginSchema,
        ]));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.ReleaseActionsSchema = z.lazy(() => z.object({
            method: z.literal('input.releaseActions'),
            params: Input.ReleaseActionsParametersSchema,
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.ReleaseActionsParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext$1.BrowsingContextSchema,
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.SetFilesSchema = z.lazy(() => z.object({
            method: z.literal('input.setFiles'),
            params: Input.SetFilesParametersSchema,
        }));
    })(Input$1 || (Input$1 = {}));
    (function (Input) {
        Input.SetFilesParametersSchema = z.lazy(() => z.object({
            context: BrowsingContext$1.BrowsingContextSchema,
            element: Script$1.SharedReferenceSchema,
            files: z.array(z.string()),
        }));
    })(Input$1 || (Input$1 = {}));

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /**
     * @fileoverview Provides parsing and validator for WebDriver BiDi protocol.
     * Parser types should match the `../protocol` types.
     */
    function parseObject(obj, schema) {
        const parseResult = schema.safeParse(obj);
        if (parseResult.success) {
            return parseResult.data;
        }
        const errorMessage = parseResult.error.errors
            .map((e) => `${e.message} in ` +
            `${e.path.map((p) => JSON.stringify(p)).join('/')}.`)
            .join(' ');
        throw new InvalidArgumentException(errorMessage);
    }
    /** @see https://w3c.github.io/webdriver-bidi/#module-browser */
    var Browser;
    (function (Browser) {
        function parseRemoveUserContextParams(params) {
            return parseObject(params, Browser$1.RemoveUserContextParametersSchema);
        }
        Browser.parseRemoveUserContextParams = parseRemoveUserContextParams;
    })(Browser || (Browser = {}));
    /** @see https://w3c.github.io/webdriver-bidi/#module-network */
    var Network;
    (function (Network) {
        function parseAddInterceptParameters(params) {
            // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
            return parseObject(params, Network$1.AddInterceptParametersSchema);
        }
        Network.parseAddInterceptParameters = parseAddInterceptParameters;
        function parseContinueRequestParameters(params) {
            return parseObject(params, Network$1.ContinueRequestParametersSchema);
        }
        Network.parseContinueRequestParameters = parseContinueRequestParameters;
        function parseContinueResponseParameters(params) {
            // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
            return parseObject(params, Network$1.ContinueResponseParametersSchema);
        }
        Network.parseContinueResponseParameters = parseContinueResponseParameters;
        function parseContinueWithAuthParameters(params) {
            return parseObject(params, Network$1.ContinueWithAuthParametersSchema);
        }
        Network.parseContinueWithAuthParameters = parseContinueWithAuthParameters;
        function parseFailRequestParameters(params) {
            return parseObject(params, Network$1.FailRequestParametersSchema);
        }
        Network.parseFailRequestParameters = parseFailRequestParameters;
        function parseProvideResponseParameters(params) {
            // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
            return parseObject(params, Network$1.ProvideResponseParametersSchema);
        }
        Network.parseProvideResponseParameters = parseProvideResponseParameters;
        function parseRemoveInterceptParameters(params) {
            return parseObject(params, Network$1.RemoveInterceptParametersSchema);
        }
        Network.parseRemoveInterceptParameters = parseRemoveInterceptParameters;
        function parseSetCacheBehavior(params) {
            return parseObject(params, Network$1.SetCacheBehaviorParametersSchema);
        }
        Network.parseSetCacheBehavior = parseSetCacheBehavior;
    })(Network || (Network = {}));
    /** @see https://w3c.github.io/webdriver-bidi/#module-script */
    var Script;
    (function (Script) {
        function parseGetRealmsParams(params) {
            return parseObject(params, Script$1.GetRealmsParametersSchema);
        }
        Script.parseGetRealmsParams = parseGetRealmsParams;
        function parseEvaluateParams(params) {
            return parseObject(params, Script$1.EvaluateParametersSchema);
        }
        Script.parseEvaluateParams = parseEvaluateParams;
        function parseDisownParams(params) {
            return parseObject(params, Script$1.DisownParametersSchema);
        }
        Script.parseDisownParams = parseDisownParams;
        function parseAddPreloadScriptParams(params) {
            return parseObject(params, Script$1.AddPreloadScriptParametersSchema);
        }
        Script.parseAddPreloadScriptParams = parseAddPreloadScriptParams;
        function parseRemovePreloadScriptParams(params) {
            return parseObject(params, Script$1.RemovePreloadScriptParametersSchema);
        }
        Script.parseRemovePreloadScriptParams = parseRemovePreloadScriptParams;
        function parseCallFunctionParams(params) {
            return parseObject(params, Script$1.CallFunctionParametersSchema);
        }
        Script.parseCallFunctionParams = parseCallFunctionParams;
    })(Script || (Script = {}));
    /** @see https://w3c.github.io/webdriver-bidi/#module-browsingContext */
    var BrowsingContext;
    (function (BrowsingContext) {
        function parseActivateParams(params) {
            return parseObject(params, BrowsingContext$1.ActivateParametersSchema);
        }
        BrowsingContext.parseActivateParams = parseActivateParams;
        function parseGetTreeParams(params) {
            return parseObject(params, BrowsingContext$1.GetTreeParametersSchema);
        }
        BrowsingContext.parseGetTreeParams = parseGetTreeParams;
        function parseNavigateParams(params) {
            return parseObject(params, BrowsingContext$1.NavigateParametersSchema);
        }
        BrowsingContext.parseNavigateParams = parseNavigateParams;
        function parseReloadParams(params) {
            return parseObject(params, BrowsingContext$1.ReloadParametersSchema);
        }
        BrowsingContext.parseReloadParams = parseReloadParams;
        function parseCreateParams(params) {
            return parseObject(params, BrowsingContext$1.CreateParametersSchema);
        }
        BrowsingContext.parseCreateParams = parseCreateParams;
        function parseCloseParams(params) {
            return parseObject(params, BrowsingContext$1.CloseParametersSchema);
        }
        BrowsingContext.parseCloseParams = parseCloseParams;
        function parseCaptureScreenshotParams(params) {
            return parseObject(params, BrowsingContext$1.CaptureScreenshotParametersSchema);
        }
        BrowsingContext.parseCaptureScreenshotParams = parseCaptureScreenshotParams;
        function parsePrintParams(params) {
            return parseObject(params, BrowsingContext$1.PrintParametersSchema);
        }
        BrowsingContext.parsePrintParams = parsePrintParams;
        function parseSetViewportParams(params) {
            return parseObject(params, BrowsingContext$1.SetViewportParametersSchema);
        }
        BrowsingContext.parseSetViewportParams = parseSetViewportParams;
        function parseTraverseHistoryParams(params) {
            return parseObject(params, BrowsingContext$1.TraverseHistoryParametersSchema);
        }
        BrowsingContext.parseTraverseHistoryParams = parseTraverseHistoryParams;
        function parseHandleUserPromptParameters(params) {
            return parseObject(params, BrowsingContext$1.HandleUserPromptParametersSchema);
        }
        BrowsingContext.parseHandleUserPromptParameters = parseHandleUserPromptParameters;
        function parseLocateNodesParams(params) {
            // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
            return parseObject(params, BrowsingContext$1.LocateNodesParametersSchema);
        }
        BrowsingContext.parseLocateNodesParams = parseLocateNodesParams;
    })(BrowsingContext || (BrowsingContext = {}));
    /** @see https://w3c.github.io/webdriver-bidi/#module-session */
    var Session;
    (function (Session) {
        function parseSubscribeParams(params) {
            return parseObject(params, Session$1.SubscriptionRequestSchema);
        }
        Session.parseSubscribeParams = parseSubscribeParams;
    })(Session || (Session = {}));
    var Input;
    (function (Input) {
        function parsePerformActionsParams(params) {
            return parseObject(params, Input$1.PerformActionsParametersSchema);
        }
        Input.parsePerformActionsParams = parsePerformActionsParams;
        function parseReleaseActionsParams(params) {
            return parseObject(params, Input$1.ReleaseActionsParametersSchema);
        }
        Input.parseReleaseActionsParams = parseReleaseActionsParams;
        function parseSetFilesParams(params) {
            return parseObject(params, Input$1.SetFilesParametersSchema);
        }
        Input.parseSetFilesParams = parseSetFilesParams;
    })(Input || (Input = {}));
    var Storage;
    (function (Storage) {
        function parseGetCookiesParams(params) {
            // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
            // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
            // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
            // `SameSite` in `src/protocol/webdriver-bidi.ts`.
            // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
            return parseObject(params, Storage$1.GetCookiesParametersSchema);
        }
        Storage.parseGetCookiesParams = parseGetCookiesParams;
        function parseSetCookieParams(params) {
            // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
            // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
            // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
            // `SameSite` in `src/protocol/webdriver-bidi.ts`.
            // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
            return parseObject(params, Storage$1.SetCookieParametersSchema);
        }
        Storage.parseSetCookieParams = parseSetCookieParams;
        function parseDeleteCookiesParams(params) {
            // Work around of `cddlconv` https://github.com/google/cddlconv/issues/19.
            // The generated schema `SameSiteSchema` in `src/protocol-parser/webdriver-bidi.ts` is
            // of type `"none" | "strict" | "lax"` which is not assignable to generated enum
            // `SameSite` in `src/protocol/webdriver-bidi.ts`.
            // TODO: remove cast after https://github.com/google/cddlconv/issues/19 is fixed.
            return parseObject(params, Storage$1.DeleteCookiesParametersSchema);
        }
        Storage.parseDeleteCookiesParams = parseDeleteCookiesParams;
    })(Storage || (Storage = {}));
    var Cdp;
    (function (Cdp) {
        const SendCommandRequestSchema = z.object({
            // Allowing any cdpMethod, and casting to proper type later on.
            method: z.string(),
            // `passthrough` allows object to have any fields.
            // https://github.com/colinhacks/zod#passthrough
            params: z.object({}).passthrough().optional(),
            session: z.string().optional(),
        });
        const GetSessionRequestSchema = z.object({
            context: BrowsingContext$1.BrowsingContextSchema,
        });
        const ResolveRealmRequestSchema = z.object({
            realm: Script$1.RealmSchema,
        });
        function parseSendCommandRequest(params) {
            return parseObject(params, SendCommandRequestSchema);
        }
        Cdp.parseSendCommandRequest = parseSendCommandRequest;
        function parseGetSessionRequest(params) {
            return parseObject(params, GetSessionRequestSchema);
        }
        Cdp.parseGetSessionRequest = parseGetSessionRequest;
        function parseResolveRealmRequest(params) {
            return parseObject(params, ResolveRealmRequestSchema);
        }
        Cdp.parseResolveRealmRequest = parseResolveRealmRequest;
    })(Cdp || (Cdp = {}));
    var Permissions;
    (function (Permissions) {
        function parseSetPermissionsParams(params) {
            return {
                // TODO: remove once "goog:" attributes are not needed.
                ...params,
                ...parseObject(params, Permissions$1.SetPermissionParametersSchema),
            };
        }
        Permissions.parseSetPermissionsParams = parseSetPermissionsParams;
    })(Permissions || (Permissions = {}));
    var Bluetooth;
    (function (Bluetooth) {
        function parseHandleRequestDevicePromptParams(params) {
            return parseObject(params, Bluetooth$1.HandleRequestDevicePromptParametersSchema);
        }
        Bluetooth.parseHandleRequestDevicePromptParams = parseHandleRequestDevicePromptParams;
    })(Bluetooth || (Bluetooth = {}));

    class BidiParser {
        // Bluetooth domain
        // keep-sorted start block=yes
        parseHandleRequestDevicePromptParams(params) {
            return Bluetooth.parseHandleRequestDevicePromptParams(params);
        }
        // keep-sorted end
        // Browser domain
        // keep-sorted start block=yes
        parseRemoveUserContextParams(params) {
            return Browser.parseRemoveUserContextParams(params);
        }
        // keep-sorted end
        // Browsing Context domain
        // keep-sorted start block=yes
        parseActivateParams(params) {
            return BrowsingContext.parseActivateParams(params);
        }
        parseCaptureScreenshotParams(params) {
            return BrowsingContext.parseCaptureScreenshotParams(params);
        }
        parseCloseParams(params) {
            return BrowsingContext.parseCloseParams(params);
        }
        parseCreateParams(params) {
            return BrowsingContext.parseCreateParams(params);
        }
        parseGetTreeParams(params) {
            return BrowsingContext.parseGetTreeParams(params);
        }
        parseHandleUserPromptParams(params) {
            return BrowsingContext.parseHandleUserPromptParameters(params);
        }
        parseLocateNodesParams(params) {
            return BrowsingContext.parseLocateNodesParams(params);
        }
        parseNavigateParams(params) {
            return BrowsingContext.parseNavigateParams(params);
        }
        parsePrintParams(params) {
            return BrowsingContext.parsePrintParams(params);
        }
        parseReloadParams(params) {
            return BrowsingContext.parseReloadParams(params);
        }
        parseSetViewportParams(params) {
            return BrowsingContext.parseSetViewportParams(params);
        }
        parseTraverseHistoryParams(params) {
            return BrowsingContext.parseTraverseHistoryParams(params);
        }
        // keep-sorted end
        // CDP domain
        // keep-sorted start block=yes
        parseGetSessionParams(params) {
            return Cdp.parseGetSessionRequest(params);
        }
        parseResolveRealmParams(params) {
            return Cdp.parseResolveRealmRequest(params);
        }
        parseSendCommandParams(params) {
            return Cdp.parseSendCommandRequest(params);
        }
        // keep-sorted end
        // Input domain
        // keep-sorted start block=yes
        parsePerformActionsParams(params) {
            return Input.parsePerformActionsParams(params);
        }
        parseReleaseActionsParams(params) {
            return Input.parseReleaseActionsParams(params);
        }
        parseSetFilesParams(params) {
            return Input.parseSetFilesParams(params);
        }
        // keep-sorted end
        // Network domain
        // keep-sorted start block=yes
        parseAddInterceptParams(params) {
            return Network.parseAddInterceptParameters(params);
        }
        parseContinueRequestParams(params) {
            return Network.parseContinueRequestParameters(params);
        }
        parseContinueResponseParams(params) {
            return Network.parseContinueResponseParameters(params);
        }
        parseContinueWithAuthParams(params) {
            return Network.parseContinueWithAuthParameters(params);
        }
        parseFailRequestParams(params) {
            return Network.parseFailRequestParameters(params);
        }
        parseProvideResponseParams(params) {
            return Network.parseProvideResponseParameters(params);
        }
        parseRemoveInterceptParams(params) {
            return Network.parseRemoveInterceptParameters(params);
        }
        parseSetCacheBehavior(params) {
            return Network.parseSetCacheBehavior(params);
        }
        // keep-sorted end
        // Permissions domain
        // keep-sorted start block=yes
        parseSetPermissionsParams(params) {
            return Permissions.parseSetPermissionsParams(params);
        }
        // keep-sorted end
        // Script domain
        // keep-sorted start block=yes
        parseAddPreloadScriptParams(params) {
            return Script.parseAddPreloadScriptParams(params);
        }
        parseCallFunctionParams(params) {
            return Script.parseCallFunctionParams(params);
        }
        parseDisownParams(params) {
            return Script.parseDisownParams(params);
        }
        parseEvaluateParams(params) {
            return Script.parseEvaluateParams(params);
        }
        parseGetRealmsParams(params) {
            return Script.parseGetRealmsParams(params);
        }
        parseRemovePreloadScriptParams(params) {
            return Script.parseRemovePreloadScriptParams(params);
        }
        // keep-sorted end
        // Session domain
        // keep-sorted start block=yes
        parseSubscribeParams(params) {
            return Session.parseSubscribeParams(params);
        }
        // keep-sorted end
        // Storage domain
        // keep-sorted start block=yes
        parseDeleteCookiesParams(params) {
            return Storage.parseDeleteCookiesParams(params);
        }
        parseGetCookiesParams(params) {
            return Storage.parseGetCookiesParams(params);
        }
        parseSetCookieParams(params) {
            return Storage.parseSetCookieParams(params);
        }
    }

    /**
     * Copyright 2022 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    /** HTML source code for the user-facing Mapper tab. */
    const mapperPageSource = '<!DOCTYPE html><title>BiDi-CDP Mapper</title><style>body{font-family: Roboto,serif;font-size:13px;color:#202124;}.log{padding: 10px;font-family:Menlo, Consolas, Monaco, Liberation Mono, Lucida Console, monospace;font-size:11px;line-height:180%;background: #f1f3f4;border-radius:4px;}.pre{overflow-wrap: break-word; margin:10px;}.card{margin:60px auto;padding:2px 0;max-width:900px;box-shadow:0 1px 4px rgba(0,0,0,0.15),0 1px 6px rgba(0,0,0,0.2);border-radius:8px;}.divider{height:1px;background:#f0f0f0;}.item{padding:16px 20px;}</style><div class="card"><div class="item"><h1>BiDi-CDP Mapper is controlling this tab</h1><p>Closing or reloading it will stop the BiDi process. <a target="_blank" title="BiDi-CDP Mapper GitHub Repository" href="https://github.com/GoogleChromeLabs/chromium-bidi">Details.</a></p></div><div class="item"><div id="logs" class="log"></div></div></div></div>';
    function generatePage() {
        // If run not in browser (e.g. unit test), do nothing.
        if (!globalThis.document.documentElement) {
            return;
        }
        globalThis.document.documentElement.innerHTML = mapperPageSource;
        // Show a confirmation dialog when the user tries to leave the Mapper tab.
        globalThis.window.onbeforeunload = () => 'Closing or reloading this tab will stop the BiDi process. Are you sure you want to leave?';
    }
    function stringify(message) {
        if (typeof message === 'object') {
            return JSON.stringify(message, null, 2);
        }
        return message;
    }
    function log(logPrefix, ...messages) {
        // If run not in browser (e.g. unit test), do nothing.
        if (!globalThis.document.documentElement) {
            return;
        }
        // Skip sending BiDi logs as they are logged once by `bidi:server:*`
        if (!logPrefix.startsWith(LogType.bidi)) {
            // If `sendDebugMessage` is defined, send the log message there.
            globalThis.window?.sendDebugMessage?.(JSON.stringify({ logType: logPrefix, messages }, null, 2));
        }
        const debugContainer = document.getElementById('logs');
        if (!debugContainer) {
            return;
        }
        // This piece of HTML should be added:
        // <div class="pre">...log message...</div>
        const lineElement = document.createElement('div');
        lineElement.className = 'pre';
        lineElement.textContent = [logPrefix, ...messages].map(stringify).join(' ');
        debugContainer.appendChild(lineElement);
        if (debugContainer.childNodes.length > 400) {
            debugContainer.removeChild(debugContainer.childNodes[0]);
        }
    }

    var _a;
    class WindowBidiTransport {
        static LOGGER_PREFIX_RECV = `${LogType.bidi}:RECV ◂`;
        static LOGGER_PREFIX_SEND = `${LogType.bidi}:SEND ▸`;
        #onMessage = null;
        constructor() {
            window.onBidiMessage = (message) => {
                log(_a.LOGGER_PREFIX_RECV, message);
                try {
                    const command = _a.#parseBidiMessage(message);
                    this.#onMessage?.call(null, command);
                }
                catch (e) {
                    const error = e instanceof Error ? e : new Error(e);
                    // Transport-level error does not provide channel.
                    this.#respondWithError(message, "invalid argument" /* ErrorCode.InvalidArgument */, error, null);
                }
            };
        }
        setOnMessage(onMessage) {
            this.#onMessage = onMessage;
        }
        sendMessage(message) {
            log(_a.LOGGER_PREFIX_SEND, message);
            const json = JSON.stringify(message);
            window.sendBidiResponse(json);
        }
        close() {
            this.#onMessage = null;
            window.onBidiMessage = null;
        }
        #respondWithError(plainCommandData, errorCode, error, channel) {
            const errorResponse = _a.#getErrorResponse(plainCommandData, errorCode, error);
            if (channel) {
                this.sendMessage({
                    ...errorResponse,
                    channel,
                });
            }
            else {
                this.sendMessage(errorResponse);
            }
        }
        static #getJsonType(value) {
            if (value === null) {
                return 'null';
            }
            if (Array.isArray(value)) {
                return 'array';
            }
            return typeof value;
        }
        static #getErrorResponse(message, errorCode, error) {
            // XXX: this is bizarre per spec. We reparse the payload and
            // extract the ID, regardless of what kind of value it was.
            let messageId;
            try {
                const command = JSON.parse(message);
                if (_a.#getJsonType(command) === 'object' &&
                    'id' in command) {
                    messageId = command.id;
                }
            }
            catch { }
            return {
                type: 'error',
                id: messageId,
                error: errorCode,
                message: error.message,
            };
        }
        static #parseBidiMessage(message) {
            let command;
            try {
                command = JSON.parse(message);
            }
            catch {
                throw new Error('Cannot parse data as JSON');
            }
            const type = _a.#getJsonType(command);
            if (type !== 'object') {
                throw new Error(`Expected JSON object but got ${type}`);
            }
            // Extract and validate id, method and params.
            const { id, method, params } = command;
            const idType = _a.#getJsonType(id);
            if (idType !== 'number' || !Number.isInteger(id) || id < 0) {
                // TODO: should uint64_t be the upper limit?
                // https://tools.ietf.org/html/rfc7049#section-2.1
                throw new Error(`Expected unsigned integer but got ${idType}`);
            }
            const methodType = _a.#getJsonType(method);
            if (methodType !== 'string') {
                throw new Error(`Expected string method but got ${methodType}`);
            }
            const paramsType = _a.#getJsonType(params);
            if (paramsType !== 'object') {
                throw new Error(`Expected object params but got ${paramsType}`);
            }
            let channel = command.channel;
            if (channel !== undefined) {
                const channelType = _a.#getJsonType(channel);
                if (channelType !== 'string') {
                    throw new Error(`Expected string channel but got ${channelType}`);
                }
                // Empty string channel is considered as no channel provided.
                if (channel === '') {
                    channel = undefined;
                }
            }
            return { id, method, params, channel };
        }
    }
    _a = WindowBidiTransport;
    class WindowCdpTransport {
        #onMessage = null;
        constructor() {
            window.cdp.onmessage = (message) => {
                this.#onMessage?.call(null, message);
            };
        }
        setOnMessage(onMessage) {
            this.#onMessage = onMessage;
        }
        sendMessage(message) {
            window.cdp.send(message);
        }
        close() {
            this.#onMessage = null;
            window.cdp.onmessage = null;
        }
    }

    /**
     * Copyright 2021 Google LLC.
     * Copyright (c) Microsoft Corporation.
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     *
     * @license
     */
    generatePage();
    const mapperTabToServerTransport = new WindowBidiTransport();
    const cdpTransport = new WindowCdpTransport();
    /**
     * A CdpTransport implementation that uses the window.cdp bindings
     * injected by Target.exposeDevToolsProtocol.
     */
    const cdpConnection = new MapperCdpConnection(cdpTransport, log);
    /**
     * Launches the BiDi mapper instance.
     * @param {string} selfTargetId
     * @param options Mapper options. E.g. `acceptInsecureCerts`.
     */
    async function runMapperInstance(selfTargetId) {
        // eslint-disable-next-line no-console
        console.log('Launching Mapper instance with selfTargetId:', selfTargetId);
        const bidiServer = await BidiServer.createAndStart(mapperTabToServerTransport, cdpConnection, 
        /**
         * Create a Browser CDP Session per Mapper instance.
         */
        await cdpConnection.createBrowserSession(), selfTargetId, new BidiParser(), log);
        log(LogType.debugInfo, 'Mapper instance has been launched');
        return bidiServer;
    }
    /**
     * Set `window.runMapper` to a function which launches the BiDi mapper instance.
     * @param selfTargetId Needed to filter out info related to BiDi target.
     */
    window.runMapperInstance = async (selfTargetId) => {
        await runMapperInstance(selfTargetId);
    };

})();
//# sourceMappingURL=mapperTab.js.map

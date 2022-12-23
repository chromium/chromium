package org.chromium.content.browser.webcontents;

public interface ObserverProxyFactory {

    WebContentsObserverProxy create(WebContentsImpl webContents);

}

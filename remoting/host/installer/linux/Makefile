# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile is used by debhelper, which supplies the appropriate value of
# variables not defined here, such as DESTDIR.

ifndef SRC_DIR
  SRC_DIR = ../../../..
endif
ifndef BUILD_DIR
  BUILD_DIR = $(SRC_DIR)/out/Release
endif

CRON_DIR = $(DESTDIR)/etc/cron.daily
DOC_DIR = $(DESTDIR)/usr/share/doc/chrome-remote-desktop
SYSTEMD_UNIT_DIR = $(DESTDIR)/lib/systemd/system
INSTALL_DIR = $(DESTDIR)/opt/google/chrome-remote-desktop
CHROME_NATIVE_MESSAGING_DIR = $(DESTDIR)/etc/opt/chrome/native-messaging-hosts
FIREFOX_NATIVE_MESSAGING_DIR = $(DESTDIR)/usr/lib/mozilla/native-messaging-hosts
SYSTEM_WIDE_DESKTOP_ENTRIES_DIR = $(DESTDIR)/usr/share/applications

CORE_LIBNAME = $(BUILD_DIR)/libremoting_core.so
CORE_DEBUGFILE = $(CORE_LIBNAME).debug
CRASH_UPLOADER_PROGNAME = $(BUILD_DIR)/remoting_crash_uploader
CRASH_UPLOADER_DEBUGFILE = $(CRASH_UPLOADER_PROGNAME).debug
ME2ME_PROGNAME = $(BUILD_DIR)/remoting_me2me_host
ME2ME_DEBUGFILE = $(ME2ME_PROGNAME).debug
START_PROGNAME = $(BUILD_DIR)/remoting_start_host
START_DEBUGFILE = $(START_PROGNAME).debug
SESSION_WRAPPER_PROGNAME = $(BUILD_DIR)/remoting_user_session
SESSION_WRAPPER_DEBUGFILE = $(SESSION_WRAPPER_PROGNAME).debug
ME2ME_NM_PROGNAME = $(BUILD_DIR)/remoting_native_messaging_host
ME2ME_NM_DEBUGFILE = $(ME2ME_NM_PROGNAME).debug
REMOTE_ASSISTANCE_PROGNAME = $(BUILD_DIR)/remote_assistance_host
REMOTE_ASSISTANCE_DEBUGFILE = $(REMOTE_ASSISTANCE_PROGNAME).debug
REMOTE_OPEN_URL_PROGNAME = $(BUILD_DIR)/remote_open_url
REMOTE_OPEN_URL_DEBUGFILE = $(REMOTE_OPEN_URL_PROGNAME).debug
REMOTE_WEBAUTHN_PROGNAME = $(BUILD_DIR)/remote_webauthn
REMOTE_WEBAUTHN_DEBUGFILE = $(REMOTE_WEBAUTHN_PROGNAME).debug

all:

# Make sure you list files to be installed in |inputs| in BUILD.gn, so that you
# don't accidentally miss a dependency, which may cause nondeterministic build
# behavior.
install:
	install -d "$(CRON_DIR)"
	install -d "$(DOC_DIR)"
	install -d "$(INSTALL_DIR)"
	install -d "$(INSTALL_DIR)/remoting_locales"
	install -d "$(SYSTEMD_UNIT_DIR)"
	install -d "$(CHROME_NATIVE_MESSAGING_DIR)"
	install -d "$(FIREFOX_NATIVE_MESSAGING_DIR)"
	install -d "$(SYSTEM_WIDE_DESKTOP_ENTRIES_DIR)"

	install "$(SRC_DIR)/remoting/host/linux/linux_me2me_host.py" \
	  "$(INSTALL_DIR)/chrome-remote-desktop"
	install "$(SRC_DIR)/remoting/host/installer/linux/is-remoting-session" \
	  "$(INSTALL_DIR)"
	install "$(SRC_DIR)/remoting/host/installer/linux/Xsession" \
	  "$(INSTALL_DIR)"
	install "$(SRC_DIR)/remoting/host/linux/configure_url_forwarder.py" \
	  "$(INSTALL_DIR)/configure-url-forwarder"

	install -m 0644 \
	  "$(SRC_DIR)/remoting/host/linux/pipewire.conf.template" \
	  "$(INSTALL_DIR)/pipewire.conf.template"
	install -m 0644 \
	  "$(SRC_DIR)/remoting/host/linux/pipewire-media-session.conf.template" \
	  "$(INSTALL_DIR)/pipewire-media-session.conf.template"
	install -m 0644 \
	  "$(SRC_DIR)/remoting/host/linux/pipewire-pulse.conf.template" \
	  "$(INSTALL_DIR)/pipewire-pulse.conf.template"
	install -m 0644 \
	  "$(SRC_DIR)/remoting/host/linux/wireplumber.conf.template" \
	  "$(INSTALL_DIR)/wireplumber.conf.template"

	install -m 0644 \
	  "$(BUILD_DIR)/remoting/com.google.chrome.remote_desktop.json" \
	  "$(CHROME_NATIVE_MESSAGING_DIR)/com.google.chrome.remote_desktop.json"
	install -m 0644 \
	  "$(BUILD_DIR)/remoting/com.google.chrome.remote_desktop-firefox.json" \
	  "$(FIREFOX_NATIVE_MESSAGING_DIR)/com.google.chrome.remote_desktop.json"

	install -m 0644 \
	  "$(BUILD_DIR)/remoting/com.google.chrome.remote_assistance.json" \
	  "$(CHROME_NATIVE_MESSAGING_DIR)/com.google.chrome.remote_assistance.json"
	install -m 0644 \
	  "$(BUILD_DIR)/remoting/com.google.chrome.remote_assistance-firefox.json" \
	  "$(FIREFOX_NATIVE_MESSAGING_DIR)/com.google.chrome.remote_assistance.json"
	install -m 0644 \
	  "$(BUILD_DIR)/remoting/com.google.chrome.remote_webauthn.json" \
	  "$(CHROME_NATIVE_MESSAGING_DIR)/com.google.chrome.remote_webauthn.json"

	install "$(CORE_LIBNAME)" "$(INSTALL_DIR)/libremoting_core.so"
	eu-strip -f "$(CORE_DEBUGFILE)" "$(INSTALL_DIR)/libremoting_core.so"

	install "$(CRASH_UPLOADER_PROGNAME)" "$(INSTALL_DIR)/crash-uploader"
	eu-strip -f "$(CRASH_UPLOADER_DEBUGFILE)" "$(INSTALL_DIR)/crash-uploader"

	install "$(ME2ME_PROGNAME)" \
	  "$(INSTALL_DIR)/chrome-remote-desktop-host"
	eu-strip -f "$(ME2ME_DEBUGFILE)" \
	  "$(INSTALL_DIR)/chrome-remote-desktop-host"

	install "$(START_PROGNAME)" "$(INSTALL_DIR)/start-host"
	eu-strip -f "$(START_DEBUGFILE)" "$(INSTALL_DIR)/start-host"

	install "$(SESSION_WRAPPER_PROGNAME)" "$(INSTALL_DIR)/user-session"
	eu-strip -f "$(SESSION_WRAPPER_DEBUGFILE)" \
	  "$(INSTALL_DIR)/user-session"

	install "$(ME2ME_NM_PROGNAME)" "$(INSTALL_DIR)/native-messaging-host"
	eu-strip -f "$(ME2ME_NM_DEBUGFILE)" \
	  "$(INSTALL_DIR)/native-messaging-host"

	install "$(REMOTE_ASSISTANCE_PROGNAME)" \
	  "$(INSTALL_DIR)/remote-assistance-host"
	eu-strip -f "$(REMOTE_ASSISTANCE_DEBUGFILE)" \
	  "$(INSTALL_DIR)/remote-assistance-host"

	install "$(REMOTE_OPEN_URL_PROGNAME)" \
	  "$(INSTALL_DIR)/remote-open-url"
	eu-strip -f "$(REMOTE_OPEN_URL_DEBUGFILE)" \
	  "$(INSTALL_DIR)/remote-open-url"

	install "$(REMOTE_WEBAUTHN_PROGNAME)" "$(INSTALL_DIR)/remote-webauthn"
	eu-strip -f "$(REMOTE_WEBAUTHN_DEBUGFILE)" "$(INSTALL_DIR)/remote-webauthn"

	install -m 0644 \
	  "$(BUILD_DIR)/remoting/crd-url-forwarder.desktop" \
	  "$(SYSTEM_WIDE_DESKTOP_ENTRIES_DIR)/crd-url-forwarder.desktop"

	install -m 0644 \
	  "$(BUILD_DIR)/icudtl.dat" "$(INSTALL_DIR)/icudtl.dat"

	for locale in $$(ls $(BUILD_DIR)/remoting_locales); do \
	  install "$(BUILD_DIR)/remoting_locales/$$locale" \
	    "$(INSTALL_DIR)/remoting_locales/$$locale"; \
	done

	install -m 0644 \
	  "$(SRC_DIR)/remoting/host/installer/linux/chrome-remote-desktop@.service" \
	  "$(SYSTEMD_UNIT_DIR)/chrome-remote-desktop@.service"

	# Mask the SysV-style init script under systemd so both don't end up enabled.
	ln -s /dev/null "$(SYSTEMD_UNIT_DIR)/chrome-remote-desktop.service"

	install $(BUILD_DIR)/remoting/installer/cron/chrome-remote-desktop \
	  "$(CRON_DIR)/chrome-remote-desktop"

	install -m 0644 \
          "$(BUILD_DIR)/gen/remoting/CREDITS.txt" "$(DOC_DIR)/CREDITS.txt"

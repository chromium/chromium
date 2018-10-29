# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import urlparse

# Valid expectation conditions are:
#
# Operating systems (all of these are mutually exclusive, so don't
# specify both "android" and "m" in the same expectation, for
# example):
#
#     win, xp, vista, win7, win8, win10
#
#     mac, leopard, snowleopard, lion, mountainlion, mavericks,
#     yosemite, sierra, highsierra, mojave
#
#     linux, chromeos
#
#     android, l, m, n, o, p, q
#
# Browser types:
#
#     android-webview-instrumentation, android-content-shell,
#     android-chromium, debug, debug_x64, release, release_x64
#
# ASAN conditions:
#     asan, no_asan
#
# Sample usage in SetExpectations in subclasses:
#   self.Fail('gl-enable-vertex-attrib.html',
#       ['mac', 'release'], bug=123)

WIN_CONDITIONS = ['xp', 'vista', 'win7', 'win8', 'win10']
MAC_CONDITIONS = ['leopard', 'snowleopard', 'lion', 'mountainlion',
                  'mavericks', 'yosemite', 'sierra', 'highsierra', 'mojave']
# These aren't expanded out into "lollipop", "marshmallow", etc.
ANDROID_CONDITIONS = ['l', 'm', 'n', 'o', 'p', 'q']

OS_CONDITIONS = ['win', 'mac', 'linux', 'chromeos', 'android'] + \
                WIN_CONDITIONS + MAC_CONDITIONS + ANDROID_CONDITIONS

BROWSER_TYPE_CONDITIONS = [
    'android-webview-instrumentation', 'android-content-shell',
    'android-chromium', 'debug', 'debug_x64', 'release', 'release_x64']

ASAN_CONDITIONS = ['asan', 'no_asan']

def _SafeLower(opt_str):
  if not opt_str:
    return opt_str
  return opt_str.lower()


class Expectation(object):
  """Represents a single expectation for a test.

  Supports conditions based on operating system (e.g., win, mac) and
  browser type (e.g. 'debug', 'release').

  The pattern, if a URL, *must* be a relative URL. Absolute URLs
  (e.g. starting with a scheme like http://, https://, file://) are
  not allowed. A ValueError is raised if one is passed to __init__.

  Subclass this class and call super.__init__ last in your constructor
  in order to add new user-defined conditions. The conditions are
  parsed at the end of this class's constructor, so be careful not to
  overwrite the results of the constructor call!

  """

  def __init__(self, expectation, pattern, conditions=None, bug=None):
    self.expectation = expectation.lower()
    if pattern.find('://') > 0:
      raise ValueError('Absolute URLs are not allowed in patterns')
    self.pattern = pattern
    self.bug = bug

    self.os_conditions = []
    self.browser_conditions = []
    self.asan_conditions = []

    if conditions:
      for c in conditions:
        self.ParseCondition(c)

  def ParseCondition(self, condition):
    """Parses a single test expectation condition.

    Can be overridden to handle new types of conditions. Call the
    superclass's implementation of ParseCondition at the end of your
    subclass if you don't handle the condition. The base
    implementation will raise an exception if the condition is
    unsupported.

    Valid expectation conditions are listed in the comments at the top
    of this file.

    Sample usage in SetExpectations in subclasses:
      self.Fail('gl-enable-vertex-attrib.html',
         ['mac', 'release'], bug=123)

    """
    cl = condition.lower()
    if cl in OS_CONDITIONS:
      self.os_conditions.append(cl)
    elif cl in BROWSER_TYPE_CONDITIONS:
      self.browser_conditions.append(cl)
    elif cl in ASAN_CONDITIONS:
      self.asan_conditions.append(cl)
    else:
      raise ValueError('Unknown expectation condition: "%s"' % cl)


class TestExpectations(object):
  """A class which defines the expectations for a test execution."""

  def __init__(self, url_prefixes=None, is_asan=False):
    self._expectations = []
    self._url_prefixes = []
    # The browser doesn't know whether it was built with ASAN or not;
    # only the surrounding environment knows that. Tests which care to
    # support ASAN-specific expectations have to tell the
    # TestExpectations object about this during its construction.
    self._is_asan = is_asan
    self._skip_matching_names = False
    self._built_expectation_cache = True
    self._ClearExpectationsCache()
    self.SetExpectations()
    if url_prefixes:
      for p in url_prefixes:
        self._url_prefixes.append(p)

  def SetExpectations(self):
    """Called on creation. Override to set up custom expectations."""
    pass

  def Fail(self, pattern, conditions=None, bug=None):
    self._Expect('fail', pattern, conditions, bug)

  def Skip(self, pattern, conditions=None, bug=None):
    self._Expect('skip', pattern, conditions, bug)

  def _Expect(self, expectation, pattern, conditions=None, bug=None):
    self._AddExpectation(self.CreateExpectation(expectation, pattern,
                                                conditions, bug))

  def _AddExpectation(self, expectation):
    '''Call this to add an expectation to the set.

    For use only by this class and subclasses. Do not call this directly.'''
    self._expectations.append(expectation)
    self._ClearExpectationsCache()


  def CreateExpectation(self, expectation, pattern, conditions=None,
                        bug=None):
    return Expectation(expectation, pattern, conditions, bug)

  def _ClearExpectationsCache(self):
    if self._built_expectation_cache:
      # Only those expectations which contain no wildcard characters
      # (those which the fnmatch module would expand).
      self._expectations_by_pattern = {}
      # The remaining expectations which require expansion.
      self._expectations_with_wildcards = []
      self._built_expectation_cache = False

  def ClearExpectationsCacheForTesting(self):
    '''For use only by unit tests.'''
    self._ClearExpectationsCache()

  def _HasWildcardCharacters(self, input_string):
    # Could make this more precise.
    return '*' in input_string or '+' in input_string

  def _BuildExpectationsCache(self, browser):
    # Turn off name matching while building the cache.
    self._skip_matching_names = True
    expectations_with_collisions = []
    for e in self._expectations:
      if self._ExpectationAppliesToTest(e, browser, None, None):
        if self._HasWildcardCharacters(e.pattern):
          self._expectations_with_wildcards.append(e)
        else:
          if e.pattern in self._expectations_by_pattern:
            # This is a fatal error. Report it after building the cache.
            expectations_with_collisions.append(e.pattern)
          self._expectations_by_pattern[e.pattern] = e
    self._built_expectation_cache = True
    self._skip_matching_names = False
    if expectations_with_collisions:
      raise Exception('FATAL ERROR: the following tests had colliding ' +
                      'expectations in the test expectations: ' +
                      ' '.join(expectations_with_collisions))

  def _GetNormalizedURL(self, url, browser):
    # Telemetry uses backslashes in its file:// URLs on Windows,
    # breaking matching of test expectations.
    if not browser.platform.GetOSName() == 'win':
      return url
    return url.replace('\\', '/')

  def _GetURLPath(self, url, browser):
    normalized_url = self._GetNormalizedURL(url, browser)
    components = urlparse.urlsplit(normalized_url)
    # For compatibility, the file:// scheme must be treated specially.
    # The top-level directory shows up in the netloc portion of the URL.
    if components[0] == 'file':
      url_path = components[1] + components[2]
    else:
      url_path = components[2]
    # Chop any leading slash since the expectations used by this class
    # assume that.
    if url_path and url_path[0] == '/':
      url_path = url_path[1:]
    # Python's urlsplit doesn't seem to handle query arguments for
    # file:// URLs properly. Split them off manually.
    query_index = url_path.find('?')
    if query_index > 0:
      url_path = url_path[0:query_index]
    # Look for the URL prefixes specified at construction time, and
    # trim the first one found, if any.
    if self._url_prefixes:
      for p in self._url_prefixes:
        if url_path.startswith(p):
          url_path = url_path[len(p):]
          break
    return url_path

  def _GetExpectationObjectForTest(self, browser, test_url, test_name):
    if not self._built_expectation_cache:
      self._BuildExpectationsCache(browser)
    # First attempt to look up by the test's URL or name.
    e = None
    # Relative URL (common case).
    url_path = self._GetURLPath(test_url, browser)
    if url_path:
      e = self._expectations_by_pattern.get(url_path)
    if e:
      return e
    if test_name:
      e = self._expectations_by_pattern.get(test_name)
    if e:
      return e
    # Fall back to scanning through the expectations containing
    # wildcards.
    for e in self._expectations_with_wildcards:
      if self._ExpectationAppliesToTest(e, browser, test_url, test_name):
        return e
    return None

  def GetAllNonWildcardExpectations(self):
    return [e for e in self._expectations
              if not self._HasWildcardCharacters(e.pattern)]


  def GetExpectationForTest(self, browser, test_url, test_name):
    '''Fetches the expectation that applies to the given test.

    The implementation of this function performs significant caching
    based on the browser's parameters, which are expected to remain
    unchanged from call to call. If this is not true, the method
    ClearExpectationsCacheForTesting is available to clear the cache;
    but file a bug if this is needed for any reason but testing.
    '''
    e = self._GetExpectationObjectForTest(browser, test_url, test_name)
    if e:
      return e.expectation
    return 'pass'

  def _ExpectationAppliesToTest(
      self, expectation, browser, test_url, test_name):
    """Defines whether the given expectation applies to the given test.

    Override this in subclasses to add more conditions. Call the
    superclass's implementation first, and return false if it returns
    false. Subclasses must not consult the test's URL or name; that is
    the responsibility of the base class.

    Args:
      expectation: an instance of a subclass of Expectation, created
          by a call to CreateExpectation.
      browser: the currently running browser.
      test_url: a string containing the current test's URL. May be
          None if _skip_matching_names is True.
      test_name: a string containing the current test's name,
          including the empty string. May be None if
          _skip_matching_names is True.
    """
    # While building the expectations cache we need to match
    # everything except the test's name or URL.
    if not self._skip_matching_names:
      if test_url is None or test_name is None:
        raise ValueError('Neither test_url nor test_name may be None')

      # Relative URL.
      if not fnmatch.fnmatch(self._GetURLPath(test_url, browser),
                             expectation.pattern):
        # Name.
        if not (test_name and fnmatch.fnmatch(test_name, expectation.pattern)):
          return False

    platform = browser.platform
    os_matches = (
      not expectation.os_conditions or
      _SafeLower(platform.GetOSName()) in expectation.os_conditions or
      _SafeLower(platform.GetOSVersionName()) in expectation.os_conditions)

    browser_matches = (
      (not expectation.browser_conditions) or
      _SafeLower(browser.browser_type) in expectation.browser_conditions)

    asan_matches = (
      (not expectation.asan_conditions) or
      ('asan' in expectation.asan_conditions and self._is_asan) or
      ('no_asan' in expectation.asan_conditions and not self._is_asan))

    return os_matches and browser_matches and asan_matches

# Adding/Modifying DoH providers

Chrome performs autoupgrade of Classic DNS resolvers to equivalent same-provider
DoH servers using the mapping encoded in
[`DohProviderEntry::GetList()`](/net/dns/public/doh_provider_entry.cc).

[TOC]

## Requesting provider list changes

Official representatives of a DoH provider can request the addition of or
modifications to the entry for their service. See the
[separate documentation](https://docs.google.com/document/d/128i2YTV2C7T6Gr3I-81zlQ-_Lprnsp24qzy_20Z1Psw)
for the criteria and process.

After following the process, if approved, the addition or modification will be
made by a member of the Chromium team.

## Modifying the DoH provider list

*** note
All modifications to the DoH provider list must be accompanied by a request in
the Chromium bug tracker in the
[DoH component](https://bugs.chromium.org/p/chromium/issues/list?q=component:Internals%3ENetwork%3EDoH),
and that request must be approved by the Chrome team.

It is generally expected that the actual code change will be performed by the
Chrome team on approval of the request.
***

1.  Ensure the request bug has been approved with a comment from a member of the
    Chrome team indicating the approval in the bug. Confirm whether the approved
    request is for an autoupgrade mapping, inclusion in the Chrome "Secure DNS"
    settings UI, or both.

    It is not necessary for the author or reviewer of the CL modifying the
    provider list to verify any aspects of the provider criteria, including
    ownership of hostnames. That is the responsibility of the Chrome team member
    who approved the request bug.
1.  Add or modify the [`DohProviderEntry`](/net/dns/public/doh_provider_entry.h)
    entry in
    [`DohProviderEntry::GetList()`](/net/dns/public/doh_provider_entry.cc).
    *   `provider` is a unique string label used to identify the specific entry.
        It may be used for provider-specific metrics data sent to Google. The
        `provider` string should be camelcase, and for cases where a single
        organization runs multiple servers/varieties, the overall organization
        name should go before the variety-specific name. For example, if
        ExampleOrg has both filtered and unfiltered servers, they may have two
        provider list entries with the `provider` strings "ExampleOrgFiltered"
        and "ExampleOrgUnfiltered".
    *   `feature` is used by experiments to control an individual provider. When
        the feature is disabled, either by default or remotely by an experiment
        config, the provider is not eligible for autoupgrade and it will not be
        listed in the "Secure DNS" settings.
    *   `ip_addresses` is the list of Classic DNS server IP addresses that are
        eligible for upgrade to the provider\'s DoH server. The addresses do not
        need to be unique within the overall provider list. If multiple DoH
        provider entries contain the same `ip_addresses` entry, the DoH servers
        for all containing provider entries could become available for use if
        Chrome detects that IP address configured. `ip_addresses` may also be
        empty if a provider is only available in "Secure DNS" settings and not
        for autoupgrade.
    *   `dns_over_tls_hostnames` is used for autoupgrade from DoT to DoH. May
        be used on platforms where Chrome can recognize configured DoT servers.
        Similar to `ip_addresses` in that it may be empty or non-unique. Note
        that addition/modification requests in the bug tracker often forget to
        mention DoT hostnames, so be sure to ask about it if you suspect a DoH
        provider may have an equivalent DoT endpoint.
    *   `dns_over_https_template` is the URI template of the DoH server. It is
        formatted according to [RFC6570](https://tools.ietf.org/html/rfc6570)
        and [RFC8484](https://tools.ietf.org/html/rfc8484). If the template
        contains a single `dns` variable, Chrome will perform GET requests, and
        if the template contains no variables, Chrome will perform POST
        requests. Confirm this matches with the provider's GET/POST preference
        in the bug tracker request.
    *   `ui_name` is the name that will be displayed to users in the "Secure
        DNS" settings. It is only needed for providers that will be listed in
        the settings. Confirm that the name conforms to the rules in the
        [criteria document](https://docs.google.com/document/d/128i2YTV2C7T6Gr3I-81zlQ-_Lprnsp24qzy_20Z1Psw/edit#heading=h.l3wtx3cufz78).

        ***promo
        Note that all `ui_name` values are currently ASCII-only strings. While
        non-ASCII names make sense, especially for region-specific providers,
        and there is no known issue with using such names, support has not been
        thoroughly tested. If adding a non-ASCII name, take extra care to test
        that it displays correctly in settings UIs for all platforms.
        ***
    *   `privacy_policy` is a URL to the privacy policy page for the provider.
        It is only needed for providers that will be listed in the settings.
    *   `display_globally` sets whether or not a provider will appear in the
        "Secure DNS" settings globally for all users. This flag is only expected
        to be set for a small number of providers.
    *   `display_countries` sets any specific countries in which the provider
        should appear in "Secure DNS" settings. Should be empty if
        `display_globally` is set. Format is the two-letter ISO 3166-1 country
        codes. The provider will be displayed to users when the OS region
        settings consider the OS to be in one of the given countries.
    *   `logging_level` should be set to `kExtra` for any entry for which
        logging/monitoring/etc is especially important. This should be the case
        only for the couple most-used providers in the list, newly-entered
        providers with some risk of issues, or providers with a history of
        issues requiring that provider to be disabled for auto upgrade.
1.  Manually test the new addition/modification.
    *** promo
    If running tests on enterprise-maintained machines, DoH may be disabled,
    leading to DoH tests always failing and the "Secure DNS" settings being
    disabled. In such cases, a strategic local-only modification to
    [`StubResolverConfigReader`](/chrome/browser/net/stub_resolver_config_reader.cc)
    may be necessary to bypass the disabling.
    ***

    *   After making any additions or modifications to the provider list, run
        the DoH browser tests:
        ```shell
        browser_tests.exe --run-manual \
        --gtest_filter= DohBrowserParameterizedTest/DohBrowserTest.*
        ```
        Investigate any failures, especially concerning the modified
        provider(s).

        For new providers, repeat the test 25-100 times (exercise judgment on
        how much scrutiny is necessary) for the specific provider to ensure the
        provider is reliable:
        ```shell
        browser_tests.exe --run-manual \
        --gtest_filter= DohBrowserParameterizedTest/DohBrowserTest.MANUAL_ExternalDohServers/PROVIDER_ID_HERE \
        --gtest_repeat=100
        ```

    *   If adding/modifying a provider intended for display in "Secure DNS"
        settings, load up the settings page and select/deselect the provider
        followed by making some simple test requests to ensure it functions
        correctly.

        If the provider is only intended to be displayed in specific countries,
        test the settings inside and outside those countries by modifying the OS
        region settings and ensuring the entry only displays for the correct
        regions. On Windows 10, this setting is found under
        "Time & Language" > "Region" > "Country or region"

        *** aside
        TODO: Document region settings for other operating systems.
        ***
1.  Send the list modification code review to a reviewer in
    [DNS OWNERS](/net/dns/OWNERS), or if no DNS owners are available, to a
    reviewer in [net OWNERS](/net/OWNERS). The reviewer should confirm that the
    process defined in this document has been followed, especially that the bug
    tracker request has been properly approved.
1.  If a provider must be initially disabled or partially disabled, e.g. because
    a provider with significant usage has requested a gradual controlled
    rollout, a Googler must:
    * Create a launch bug, e.g. the [Cox DoH provider launch
      bug](https://crbug.com/1303146).
    * Create a Finch config to roll out each DoH provider, e.g.
      `DnsOverHttpsCox.gcl`.
        * Ensure that the provider's `DohProviderEntry::feature` is disabled by
          default and is enabled by the Finch config.
        * Before landing the Finch config, make the corresponding changes in
          [fieldtrial_testing_config.json](/testing/variations/fieldtrial_testing_config.json).
        * Once the DoH provider's feature has been launched and the Finch
          experiment has expired, `DohProviderEntry::feature` should be enabled
          by default.

## Dynamic control

DoH providers, especially new ones, may have service interruptions or
performance degradation to the point that it's necessary to disable their
autoupgrade feature.

If the malfunctioning DoH provider is still in the middle of a gradual rollout,
Googlers may dynamically disable the provider by modifying its experiment config
(`DnsOverHttps${ProviderName}.gcl`).

Otherwise, if the provider's autoupgrade feature has already been launched,
Googlers should create a new "kill switch config" rather than reuse the expired
gradual rollout config. Follow the guidance at
[go/finch-killswitch](http://go/finch-killswitch).

*** aside
If a user has selected a provider via the "Secure DNS" settings and that
provider becomes disabled, the UI option will disappear from the dropdown but
selection will convert to a custom text-box entry for the same provider and
continue to be used for that user.
***

import pytest

from webdriver.bidi.modules.storage import BrowsingContextPartitionDescriptor

from ... import recursive_compare

pytestmark = pytest.mark.asyncio


async def test_partition(
    bidi_session,
    top_context,
    new_tab,
    test_page,
    test_page_cross_origin,
    domain_value,
    add_cookie,
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_cross_origin, wait="complete"
    )
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=test_page, wait="complete"
    )

    cookie1_name = "foo"
    cookie1_value = "bar"
    await add_cookie(new_tab["context"], cookie1_name, cookie1_value)

    cookie2_name = "foo_2"
    cookie2_value = "bar_2"
    await add_cookie(top_context["context"], cookie2_name, cookie2_value)

    cookies = await bidi_session.storage.get_cookies()

    assert cookies["partitionKey"] == {}
    assert len(cookies["cookies"]) == 2
    # Provide consistent cookies order.
    (cookie_1, cookie_2) = sorted(cookies["cookies"], key=lambda c: c["domain"])
    recursive_compare(
        {
            "domain": domain_value(),
            "httpOnly": False,
            "name": cookie1_name,
            "path": "/webdriver/tests/support",
            "sameSite": "none",
            "secure": False,
            "size": 6,
            "value": {"type": "string", "value": cookie1_value},
        },
        cookie_2,
    )
    recursive_compare(
        {
            "domain": domain_value("alt"),
            "httpOnly": False,
            "name": cookie2_name,
            "path": "/webdriver/tests/support",
            "sameSite": "none",
            "secure": False,
            "size": 10,
            "value": {"type": "string", "value": cookie2_value},
        },
        cookie_1,
    )


async def test_partition_context(
    bidi_session, new_tab, test_page, domain_value, add_cookie
):
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=test_page, wait="complete"
    )

    cookie_name = "foo"
    cookie_value = "bar"
    await add_cookie(new_tab["context"], cookie_name, cookie_value)

    # Check that added cookies are present on the right context
    cookies = await bidi_session.storage.get_cookies(
        partition=BrowsingContextPartitionDescriptor(new_tab["context"])
    )

    # `partitionKey` here might contain `sourceOrigin` for certain browser implementation,
    # so use `recursive_compare` to allow additional fields to be present.
    recursive_compare({"partitionKey": {}}, cookies)

    assert len(cookies["cookies"]) == 1
    recursive_compare(
        {
            "domain": domain_value(),
            "httpOnly": False,
            "name": cookie_name,
            "path": "/webdriver/tests/support",
            "sameSite": "none",
            "secure": False,
            "size": 6,
            "value": {"type": "string", "value": cookie_value},
        },
        cookies["cookies"][0],
    )

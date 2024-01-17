import pytest
from webdriver.bidi.modules.network import NetworkStringValue
from webdriver.bidi.modules.storage import PartialCookie, BrowsingContextPartitionDescriptor
from .. import assert_cookie_is_set

pytestmark = pytest.mark.asyncio

COOKIE_NAME = 'SOME_COOKIE_NAME'
COOKIE_VALUE = 'SOME_COOKIE_VALUE'


@pytest.mark.parametrize(
    "protocol",
    [
        "http",
        "https",
    ]
)
async def test_page_protocols(bidi_session, top_context, get_test_page, origin, domain_value, protocol):
    # Navigate to a page with a required protocol.
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=get_test_page(protocol=protocol), wait="complete"
    )

    source_origin = origin(protocol)
    partition = BrowsingContextPartitionDescriptor(top_context["context"])

    set_cookie_result = await bidi_session.storage.set_cookie(
        cookie=PartialCookie(
            name=COOKIE_NAME,
            value=NetworkStringValue(COOKIE_VALUE),
            domain=domain_value(),
            secure=True
        ),
        partition=partition)

    assert set_cookie_result == {
        'partitionKey': {
            'sourceOrigin': source_origin
        },
    }

    # Assert the cookie is actually set.
    await assert_cookie_is_set(bidi_session, name=COOKIE_NAME, str_value=COOKIE_VALUE,
                               domain=domain_value(), origin=source_origin)

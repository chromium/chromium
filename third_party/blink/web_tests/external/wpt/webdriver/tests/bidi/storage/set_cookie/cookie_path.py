import pytest
from webdriver.bidi.modules.network import NetworkStringValue
from webdriver.bidi.modules.storage import PartialCookie, BrowsingContextPartitionDescriptor
from .. import assert_cookie_is_set

pytestmark = pytest.mark.asyncio

COOKIE_NAME = 'SOME_COOKIE_NAME'
COOKIE_VALUE = 'SOME_COOKIE_VALUE'


@pytest.mark.parametrize(
    "path",
    [
        "/",
        "/some_path",
        "/some/nested/path",
    ]
)
async def test_cookie_path(bidi_session, top_context, test_page, origin, domain_value, path):
    # Navigate to a secure context.
    await bidi_session.browsing_context.navigate(context=top_context["context"], url=test_page, wait="complete")

    source_origin = origin()
    partition = BrowsingContextPartitionDescriptor(top_context["context"])

    set_cookie_result = await bidi_session.storage.set_cookie(
        cookie=PartialCookie(
            name=COOKIE_NAME,
            value=NetworkStringValue(COOKIE_VALUE),
            domain=domain_value(),
            path=path,
            secure=True
        ),
        partition=partition)

    assert set_cookie_result == {
        'partitionKey': {
            'sourceOrigin': source_origin
        },
    }

    await assert_cookie_is_set(bidi_session, name=COOKIE_NAME, str_value=COOKIE_VALUE, path=path,
                               domain=domain_value(), origin=source_origin)

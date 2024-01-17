import pytest
from webdriver.bidi.modules.storage import BrowsingContextPartitionDescriptor
from .. import assert_cookie_is_set, create_cookie

pytestmark = pytest.mark.asyncio


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
        cookie=create_cookie(domain=domain_value(), path=path),
        partition=partition)

    assert set_cookie_result == {
        'partitionKey': {
            'sourceOrigin': source_origin
        },
    }

    await assert_cookie_is_set(bidi_session, path=path, domain=domain_value(), origin=source_origin)

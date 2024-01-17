import pytest
from .. import create_cookie
from webdriver.bidi.modules.storage import BrowsingContextPartitionDescriptor
import webdriver.bidi.error as error

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "path",
    [
        ""
        "no_leading_forward_slash"
    ]
)
async def test_path_invalid_values(bidi_session, top_context, test_page, origin, domain_value, path):
    # Navigate to a secure context.
    await bidi_session.browsing_context.navigate(context=top_context["context"], url=test_page, wait="complete")

    partition = BrowsingContextPartitionDescriptor(top_context["context"])

    with pytest.raises(error.UnableToSetCookieException):
        await bidi_session.storage.set_cookie(
            cookie=create_cookie(domain=domain_value(), path=path),
            partition=partition)

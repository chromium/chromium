import pytest
from webdriver.bidi.modules.storage import BrowsingContextPartitionDescriptor, StorageKeyPartitionDescriptor
from .. import assert_cookie_is_set, create_cookie

pytestmark = pytest.mark.asyncio


async def test_partition_context(bidi_session, top_context, test_page, origin, domain_value):
    # Navigate to a secure context.
    await bidi_session.browsing_context.navigate(context=top_context["context"], url=test_page, wait="complete")

    source_origin = origin()
    partition = BrowsingContextPartitionDescriptor(top_context["context"])

    set_cookie_result = await bidi_session.storage.set_cookie(
        cookie=create_cookie(domain=domain_value()),
        partition=partition)

    assert set_cookie_result == {
        'partitionKey': {
            'sourceOrigin': source_origin
        },
    }

    await assert_cookie_is_set(bidi_session, domain=domain_value(), origin=source_origin)


async def test_partition_storage_key_source_origin(bidi_session, top_context, test_page, origin, domain_value):
    # Navigate to a secure context.
    await bidi_session.browsing_context.navigate(context=top_context["context"], url=test_page, wait="complete")

    source_origin = origin()
    partition = StorageKeyPartitionDescriptor(source_origin=source_origin)

    set_cookie_result = await bidi_session.storage.set_cookie(
        cookie=create_cookie(domain=domain_value()),
        partition=partition)

    assert set_cookie_result == {
        'partitionKey': {
            'sourceOrigin': source_origin
        },
    }

    await assert_cookie_is_set(bidi_session, domain=domain_value(), origin=source_origin)

# TODO: test `test_partition_storage_key_user_context`.

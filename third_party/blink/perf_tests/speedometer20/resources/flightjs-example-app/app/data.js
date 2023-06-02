'use strict';

define(
  function() {
    return {
      folders: ["inbox", "later", "sent", "trash"],
      contacts: [
        {
          "id": "contact_342",
          "firstName": "Michael",
          "lastName": "Smith",
          "email": "ms@proxyweb.com"
        },
        {
          "id": "contact_377",
          "firstName": "Mary",
          "lastName": "Jones",
          "email": "mary@jones.net"
        },
        {
          "id": "contact_398",
          "firstName": "Billy",
          "lastName": "Idiot",
          "email": "william_idiot@gmail.com"
        }
      ],
      mail: [
        {
          "id": "mail_2139",
          "contact_id": "contact_342",
          "folders": ["inbox"],
          "time": 1334891976104,
          "subject": "Consectetur adipiscing elit",
          "message": "Vestibulum vestibulum varius diam in iaculis. Praesent ultrices dui vitae nibh malesuada non iaculis ante vulputate. Suspendisse feugiat ultricies egestas. Aenean a odio libero. Quisque mollis leo et est euismod sit amet dignissim sapien venenatis. Morbi interdum adipiscing massa"
        },
        {
          "id": "mail_2143",
          "contact_id": "contact_377",
          "folders": ["inbox", "later"],
          "important": "true",
          "time": 1334884976104,
          "subject": "Neque porro quisquam velit!!",
          "message": "Curabitur sollicitudin mi eget sapien posuere semper. Fusce at neque et lacus luctus vulputate vehicula ac enim"
        },
        {
          "id": "mail_2154",
          "contact_id": "contact_398",
          "folders": ["inbox"],
          "important": "true",
          "unread": "true",
          "time": 1334874976199,
          "subject": "Proin egestas aliquam :)",
          "message": "Aenean nec erat id ipsum faucibus tristique. Nam blandit est lacinia turpis consectetur elementum. Nulla in risus ut sapien dignissim feugiat. Proin ultrices sodales imperdiet. Vestibulum vehicula blandit tincidunt. Vivamus posuere rhoncus orci, porta commodo mauris aliquam nec"
        },
        {
          "id": "mail_2176",
          "contact_id": "contact_377",
          "folders": ["inbox"],
          "time": 1334884976104,
          "subject": "Sed ut perspiciatis unde omnis?",
          "message": "laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt. Neque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem."
        },
        {
          "id": "mail_2191",
          "contact_id": "contact_398",
          "folders": ["inbox"],
          "unread": "true",
          "time": 1334874976199,
          "subject": "At vero eos et accusamus!",
          "message": "Nam libero tempore, cum soluta nobis est eligendi optio cumque nihil impedit quo minus id quod maxime placeat facere possimus, omnis voluptas assumenda est, omnis dolor repellendus. Temporibus autem quibusdam et aut officiis debitis aut rerum necessitatibus saepe eveniet ut et voluptates repudiandae sint et molestiae non recusandae. Itaque earum rerum hic tenetur a sapiente delectus, ut aut reiciendis voluptatibus maiores alias consequatur aut perferendis doloribus asperiores repellat"
        },
        {
          "id": "mail_2203",
          "contact_id": "contact_377",
          "folders": ["later"],
          "important": "true",
          "time": 1334874576199,
          "subject": "Mi netus convallis",
          "message": "Egestas morbi at. Curabitur aliquet et commodo nonummy, aliquam quis arcu, sed pellentesque vitae molestie mattis magna, in eget, risus nulla vivamus vulputate"
        },
        {
          "id": "mail_2212",
          "contact_id": "contact_398",
          "folders": ["sent"],
          "time": 1334874579867,
          "subject": "Fusce tristique pretium eros a gravida",
          "message": "Proin malesuada"
        },
        {
          "id": "mail_2021",
          "contact_id": "contact_342",
          "folders": ["trash"],
          "time": 1134874579824,
          "subject": "Phasellus vitae interdum nulla.",
          "message": "Pellentesque quam eros, mollis quis vulputate eget, pellentesque nec ipsum. Cras dignissim fringilla ligula, ac ullamcorper dui convallis blandit. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Etiam id nunc ac orci hendrerit faucibus vel in ante. Mauris nec est turpis, ut fringilla mi. Suspendisse vel tortor at nulla facilisis venenatis in sit amet ligula."
        },
        {
          "id": "mail_1976",
          "contact_id": "contact_377",
          "folders": ["trash"],
          "time": 1034874579813,
          "subject": "Fusce tristique pretium :(",
          "message": "aliquam quis arcu."
        }
      ]
    };
    return data;
  }
);

